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

namespace
{
    constexpr double kTargetSampleRate = 44100.0;
    constexpr AVAudioChannelCount kTargetChannels = 2;
    constexpr AVAudioFrameCount kTapBufferFrames = 1024;
    constexpr AVAudioFrameCount kConverterSlackFrames = 64;
}

AVAudioFormat *SVMakeTargetAudioFormat(void)
{
    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                            sampleRate:kTargetSampleRate
                                              channels:kTargetChannels
                                           interleaved:YES];
}

void SVConvertAndPushPCM(SVContext *context, AVAudioConverter *converter, AVAudioFormat *targetFormat, AVAudioPCMBuffer *buffer, double ratio)
{
    const AVAudioFrameCount capacity = static_cast<AVAudioFrameCount>(buffer.frameLength * ratio) + kConverterSlackFrames;
    AVAudioPCMBuffer *converted = [[AVAudioPCMBuffer alloc] initWithPCMFormat:targetFormat frameCapacity:capacity];
    __block BOOL consumed = NO;

    AVAudioConverterOutputStatus status = [converter convertToBuffer:converted
                                                               error:nil
                                                  withInputFromBlock:^AVAudioBuffer *(AVAudioPacketCount packets, AVAudioConverterInputStatus *outStatus) {
        if (consumed)
        {
            *outStatus = AVAudioConverterInputStatus_NoDataNow;
            return nil;
        }

        consumed = YES;
        *outStatus = AVAudioConverterInputStatus_HaveData;
        return buffer;
    }];

    if (status == AVAudioConverterOutputStatus_Error || converted.frameLength == 0)
        return;

    [context pushPCMFloat:converted.floatChannelData[0] frames:converted.frameLength channels:targetFormat.channelCount];
}

@interface SVAudioInput ()

@property (nonatomic, weak, readwrite, nullable) SVContext *context;
@property (nonatomic, assign, readwrite, getter=isRunning) BOOL running;

@end

@implementation SVAudioInput
{
    AVAudioEngine *_engine;
    AVAudioConverter *_converter;
    AVAudioFormat *_targetFormat;
}

- (instancetype)initWithContext:(SVContext *)context
{
    self = [super init];

    if (self != nil)
        _context = context;

    return self;
}

- (void)dealloc
{
    [self stop];
}

- (BOOL)startWithError:(NSError **)error
{
    if (self.running)
        return YES;

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

    AVAudioFormat *targetFormat = SVMakeTargetAudioFormat();
    AVAudioConverter *converter = [[AVAudioConverter alloc] initFromFormat:inputFormat toFormat:targetFormat];

    if (converter == nil)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorIO, @"no converter from the input format to 44100 Hz stereo float");
        return NO;
    }

    __weak SVContext *weakContext = self.context;
    const double ratio = kTargetSampleRate / inputFormat.sampleRate;

    @try
    {
        [input installTapOnBus:0 bufferSize:kTapBufferFrames format:inputFormat block:^(AVAudioPCMBuffer *buffer, AVAudioTime *when) {
            SVContext *context = weakContext;

            if (context == nil || buffer.frameLength == 0)
                return;

            SVConvertAndPushPCM(context, converter, targetFormat, buffer, ratio);
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
    _converter = converter;
    _targetFormat = targetFormat;
    self.running = YES;
    return YES;
}

- (void)stop
{
    if (_engine == nil)
        return;

    [_engine.inputNode removeTapOnBus:0];
    [_engine stop];
    _engine = nil;
    _converter = nil;
    _targetFormat = nil;
    self.running = NO;
}

@end
