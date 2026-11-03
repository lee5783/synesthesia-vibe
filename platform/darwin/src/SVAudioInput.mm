//
//  SVAudioInput.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVAudioInput.h"
#import "SVAudioInput+Internal.h"
#import "SVContext+Internal.h"

#import <AVFoundation/AVFoundation.h>
#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <CoreAudio/AudioHardware.h>
#endif

#include <math.h>
#include <os/lock.h>
#include <stdio.h>
#include <string.h>

namespace
{
    constexpr double kTargetSampleRate = 44100.0;
    constexpr AVAudioChannelCount kTargetChannels = 2;
    constexpr AVAudioFrameCount kTapBufferFrames = 1024;
    constexpr AVAudioFrameCount kMaximumInputFrames = 8192;
    constexpr AVAudioFrameCount kConverterSlackFrames = 64;
    constexpr int kMaximumConverterPasses = 8;

    os_unfair_lock gSinkContextsLock = OS_UNFAIR_LOCK_INIT;

    NSHashTable<SVContext *>* SVSinkContexts()
    {
        static NSHashTable<SVContext *>* contexts = [NSHashTable weakObjectsHashTable];
        return contexts;
    }

    SVContext* SVLiveSinkContext()
    {
        SVContext* live = nil;
        os_unfair_lock_lock(&gSinkContextsLock);

        for (SVContext* context in SVSinkContexts())
        {
            if (context.rawContext != nullptr)
            {
                live = context;
                break;
            }
        }

        os_unfair_lock_unlock(&gSinkContextsLock);
        return live;
    }

    SVAudioAuthorizationStatus SVAuthorizationStatusFromCapture(AVAuthorizationStatus status)
    {
        switch (status)
        {
        case AVAuthorizationStatusNotDetermined: return SVAudioAuthorizationStatusNotDetermined;
        case AVAuthorizationStatusRestricted: return SVAudioAuthorizationStatusRestricted;
        case AVAuthorizationStatusDenied: return SVAudioAuthorizationStatusDenied;
        case AVAuthorizationStatusAuthorized: return SVAudioAuthorizationStatusAuthorized;
        }

        return SVAudioAuthorizationStatusUnknown;
    }
}

AVAudioFormat *SVMakeTargetAudioFormat(void)
{
    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                            sampleRate:kTargetSampleRate
                                              channels:kTargetChannels
                                           interleaved:YES];
}

void SVRegisterAudioSinkContext(SVContext *context)
{
    if (context == nil)
        return;

    os_unfair_lock_lock(&gSinkContextsLock);
    [SVSinkContexts() addObject:context];
    os_unfair_lock_unlock(&gSinkContextsLock);
}

float SVNormalizedInputGain(float gain)
{
    return isfinite(gain) && gain > 0.0f ? gain : 0.0f;
}

void SVConvertAndPushPCM(SVAudioPCMSink *sink, AVAudioConverter *converter, AVAudioConverterInputBlock inputBlock, AVAudioPCMBuffer *converted)
{
    const NSInteger channels = converted.format.channelCount;

    for (int pass = 0; pass < kMaximumConverterPasses; ++pass)
    {
        converted.frameLength = 0;
        const AVAudioConverterOutputStatus status = [converter convertToBuffer:converted error:nil withInputFromBlock:inputBlock];

        if (status == AVAudioConverterOutputStatus_Error)
            return;

        if (converted.frameLength > 0)
            [sink pushFloat:converted.floatChannelData[0] frames:converted.frameLength channels:channels];

        if (status != AVAudioConverterOutputStatus_HaveData)
            return;
    }
}

@implementation SVAudioPCMSink
{
    __weak SVContext *_context;
}

- (instancetype)initWithContext:(nullable SVContext *)context
{
    self = [super init];

    if (self != nil)
    {
        _context = context;
        _gain = 1.0f;
    }

    return self;
}

- (void)pushFloat:(float *)interleaved frames:(NSUInteger)frames channels:(NSInteger)channels
{
    const float gain = self.gain;

    if (gain != 1.0f)
    {
        const NSUInteger samples = frames * static_cast<NSUInteger>(channels);

        for (NSUInteger sample = 0; sample < samples; ++sample)
            interleaved[sample] *= gain;
    }

    SVAudioPCMHandler handler = self.handler;

    if (handler != nil)
        handler(interleaved, frames, channels);

    SVContext *context = _context;

    if (context == nil || context.rawContext == nullptr)
    {
        context = SVLiveSinkContext();
        _context = context;
    }

    [context pushPCMFloat:interleaved frames:frames channels:channels];
}

@end

@implementation SVAudioConversion
{
    AVAudioConverter *_converter;
    AVAudioPCMBuffer *_staging;
    AVAudioPCMBuffer *_converted;
    AVAudioPCMBuffer *_pendingInput;
    AVAudioConverterInputBlock _inputBlock;
}

- (nullable instancetype)initWithInputFormat:(AVAudioFormat *)inputFormat maximumInputFrames:(AVAudioFrameCount)maximumInputFrames
{
    self = [super init];

    if (self == nil)
        return nil;

    AVAudioFormat *targetFormat = SVMakeTargetAudioFormat();
    const double ratio = targetFormat.sampleRate / inputFormat.sampleRate;
    const AVAudioFrameCount convertedFrames = static_cast<AVAudioFrameCount>(ceil(maximumInputFrames * ratio)) + kConverterSlackFrames;

    _converter = [[AVAudioConverter alloc] initFromFormat:inputFormat toFormat:targetFormat];
    _staging = [[AVAudioPCMBuffer alloc] initWithPCMFormat:inputFormat frameCapacity:maximumInputFrames];
    _converted = [[AVAudioPCMBuffer alloc] initWithPCMFormat:targetFormat frameCapacity:convertedFrames];

    if (_converter == nil || _staging == nil || _converted == nil)
        return nil;

    _inputFormat = inputFormat;

    __unsafe_unretained SVAudioConversion *conversion = self;
    _inputBlock = ^AVAudioBuffer *(AVAudioPacketCount packets, AVAudioConverterInputStatus *outStatus) {
        AVAudioPCMBuffer *pending = conversion->_pendingInput;
        conversion->_pendingInput = nil;
        *outStatus = pending != nil ? AVAudioConverterInputStatus_HaveData : AVAudioConverterInputStatus_NoDataNow;
        return pending;
    };

    return self;
}

- (void)convertBuffer:(AVAudioPCMBuffer *)buffer intoSink:(SVAudioPCMSink *)sink
{
    if (buffer.frameLength == 0)
        return;

    if (buffer.frameLength > _staging.frameCapacity)
    {
        [self convertBufferList:buffer.audioBufferList intoSink:sink];
        return;
    }

    [self convertPendingBuffer:buffer intoSink:sink];
}

- (void)convertBufferList:(const AudioBufferList *)bufferList intoSink:(SVAudioPCMSink *)sink
{
    const UInt32 bytesPerFrame = _inputFormat.streamDescription->mBytesPerFrame;

    if (bufferList == nullptr || bufferList->mNumberBuffers == 0 || bytesPerFrame == 0)
        return;

    AudioBufferList *staging = _staging.mutableAudioBufferList;
    const UInt32 bufferCount = MIN(bufferList->mNumberBuffers, staging->mNumberBuffers);
    const AVAudioFrameCount totalFrames = bufferList->mBuffers[0].mDataByteSize / bytesPerFrame;
    AVAudioFrameCount offset = 0;

    while (offset < totalFrames)
    {
        const AVAudioFrameCount frames = MIN(totalFrames - offset, _staging.frameCapacity);
        const UInt32 offsetBytes = offset * bytesPerFrame;
        const UInt32 chunkBytes = frames * bytesPerFrame;

        for (UInt32 index = 0; index < bufferCount; ++index)
        {
            const AudioBuffer &source = bufferList->mBuffers[index];
            unsigned char *destination = static_cast<unsigned char *>(staging->mBuffers[index].mData);
            const UInt32 available = source.mData != nullptr && source.mDataByteSize > offsetBytes ? source.mDataByteSize - offsetBytes : 0;
            const UInt32 copied = MIN(chunkBytes, available);

            if (copied > 0)
                memcpy(destination, static_cast<const unsigned char *>(source.mData) + offsetBytes, copied);

            if (copied < chunkBytes)
                memset(destination + copied, 0, chunkBytes - copied);
        }

        _staging.frameLength = frames;
        [self convertPendingBuffer:_staging intoSink:sink];
        offset += frames;
    }
}

- (void)convertPendingBuffer:(AVAudioPCMBuffer *)buffer intoSink:(SVAudioPCMSink *)sink
{
    _pendingInput = buffer;
    SVConvertAndPushPCM(sink, _converter, _inputBlock, _converted);
    _pendingInput = nil;
}

@end

@interface SVAudioInput ()

@property (nonatomic, weak, readwrite, nullable) SVContext *context;
@property (nonatomic, assign, readwrite, getter=isRunning) BOOL running;

@end

@implementation SVAudioInput
{
    AVAudioEngine *_engine;
    SVAudioConversion *_conversion;
    SVAudioPCMSink *_sink;
#if TARGET_OS_OSX
    AudioObjectPropertyListenerBlock _defaultInputListener;
#endif
}

+ (SVAudioAuthorizationStatus)authorizationStatus
{
    return SVAuthorizationStatusFromCapture([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio]);
}

+ (void)requestAccessWithCompletion:(void (^)(SVAudioAuthorizationStatus status))completion
{
    void (^handler)(SVAudioAuthorizationStatus) = [completion copy];

    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (handler != nil)
                handler([SVAudioInput authorizationStatus]);
        });
    }];
}

- (instancetype)initWithContext:(SVContext *)context
{
    self = [super init];

    if (self != nil)
    {
        _context = context;
        _sink = [[SVAudioPCMSink alloc] initWithContext:context];
    }

    return self;
}

- (void)dealloc
{
    [self stop];
}

- (float)inputGain
{
    return _sink.gain;
}

- (void)setInputGain:(float)inputGain
{
    _sink.gain = SVNormalizedInputGain(inputGain);
}

- (nullable SVAudioPCMHandler)pcmHandler
{
    return _sink.handler;
}

- (void)setPcmHandler:(nullable SVAudioPCMHandler)pcmHandler
{
    _sink.handler = pcmHandler;
}

- (BOOL)startWithError:(NSError **)error
{
    if (self.running && _engine != nil)
        return YES;

    const SVAudioAuthorizationStatus authorization = [SVAudioInput authorizationStatus];

    if (authorization == SVAudioAuthorizationStatusDenied || authorization == SVAudioAuthorizationStatusRestricted)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorPermissionDenied, @"microphone access is denied");
        return NO;
    }

#if TARGET_OS_IPHONE
    AVAudioSession *session = [AVAudioSession sharedInstance];

    if (![session.category isEqualToString:AVAudioSessionCategoryRecord] && ![session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord])
    {
        if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                      withOptions:AVAudioSessionCategoryOptionMixWithOthers | AVAudioSessionCategoryOptionDefaultToSpeaker
                            error:error])
            return NO;
    }

    if (![session setActive:YES error:error])
        return NO;
#endif

    if (![self startEngineWithError:error])
        return NO;

    [self startObservingDevices];
    self.running = YES;
    return YES;
}

- (BOOL)startEngineWithError:(NSError **)error
{
    AVAudioEngine *engine = [[AVAudioEngine alloc] init];
    AVAudioInputNode *input = engine.inputNode;
    AVAudioFormat *hardwareFormat = [input inputFormatForBus:0];
    AVAudioFormat *inputFormat = [input outputFormatForBus:0];

    if (hardwareFormat.sampleRate <= 0.0 || hardwareFormat.channelCount == 0 || inputFormat.sampleRate <= 0.0 || inputFormat.channelCount == 0)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorIO, @"no audio input device is available");
        return NO;
    }

    SVAudioConversion *conversion = [[SVAudioConversion alloc] initWithInputFormat:inputFormat maximumInputFrames:kMaximumInputFrames];

    if (conversion == nil)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorIO, @"no converter from the input format to 44100 Hz stereo float");
        return NO;
    }

    SVAudioPCMSink *sink = _sink;

    @try
    {
        [input installTapOnBus:0 bufferSize:kTapBufferFrames format:inputFormat block:^(AVAudioPCMBuffer *buffer, AVAudioTime *when) {
            @autoreleasepool
            {
                [conversion convertBuffer:buffer intoSink:sink];
            }
        }];
    }
    @catch (NSException *exception)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorIO, exception.reason ?: @"no audio input device is available");
        return NO;
    }

    [engine prepare];

    if (![engine startAndReturnError:error])
    {
        [input removeTapOnBus:0];
        return NO;
    }

    _engine = engine;
    _conversion = conversion;
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(engineConfigurationDidChange:)
                                                 name:AVAudioEngineConfigurationChangeNotification
                                               object:engine];
    fprintf(stderr, "SVAudioInput: engine started %.0f Hz %u channels\n", inputFormat.sampleRate, static_cast<unsigned>(inputFormat.channelCount));
    return YES;
}

- (void)stopEngine
{
    if (_engine == nil)
        return;

    [[NSNotificationCenter defaultCenter] removeObserver:self name:AVAudioEngineConfigurationChangeNotification object:_engine];
    [_engine.inputNode removeTapOnBus:0];
    [_engine stop];
    _engine = nil;
    _conversion = nil;
}

- (void)engineConfigurationDidChange:(NSNotification *)notification
{
    __weak SVAudioInput *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf restartEngineWithReason:@"engine configuration change"];
    });
}

- (void)restartEngineWithReason:(NSString *)reason
{
    if (!self.running)
        return;

    [self stopEngine];

    NSError *error = nil;
    const BOOL restarted = [self startEngineWithError:&error];
    fprintf(stderr, "SVAudioInput: restart reason=%s %s\n", reason.UTF8String, restarted ? "ok" : (error.localizedDescription.UTF8String ?: "failed"));
}

- (void)startObservingDevices
{
#if TARGET_OS_OSX
    if (_defaultInputListener != nil)
        return;

    __weak SVAudioInput *weakSelf = self;
    AudioObjectPropertyListenerBlock listener = ^(UInt32 addressCount, const AudioObjectPropertyAddress *addresses) {
        SVAudioInput *input = weakSelf;

        if (input != nil && input->_engine == nil)
            [input restartEngineWithReason:@"default input device change"];
    };

    const AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };

    if (AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), listener) == noErr)
        _defaultInputListener = listener;
#endif
}

- (void)stopObservingDevices
{
#if TARGET_OS_OSX
    if (_defaultInputListener == nil)
        return;

    const AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), _defaultInputListener);
    _defaultInputListener = nil;
#endif
}

- (void)stop
{
    [self stopObservingDevices];
    [self stopEngine];
    self.running = NO;
}

@end
