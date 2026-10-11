//
//  SVSystemAudioInput.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVSystemAudioInput.h"
#import "SVAudioInput+Internal.h"
#import "SVContext+Internal.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <CoreAudio/AudioHardware.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>
#endif

#if TARGET_OS_OSX
namespace
{
    NSString* const kAggregateDeviceName = @"SynesthesiaVibe System Audio";

    NSError* SVMakeOSStatusError(NSString* what, OSStatus status)
    {
        return SVMakeError(SVErrorIO, [NSString stringWithFormat:@"%@ failed (OSStatus %d)", what, static_cast<int>(status)]);
    }

    NSString* SVDefaultOutputDeviceUID()
    {
        AudioObjectPropertyAddress address = { kAudioHardwarePropertyDefaultSystemOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioObjectID device = kAudioObjectUnknown;
        UInt32 size = sizeof(device);

        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, &device) != noErr || device == kAudioObjectUnknown)
            return nil;

        address.mSelector = kAudioDevicePropertyDeviceUID;
        CFStringRef uid = nullptr;
        size = sizeof(uid);

        if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &uid) != noErr || uid == nullptr)
            return nil;

        return CFBridgingRelease(uid);
    }
}
#endif

@interface SVSystemAudioInput ()

@property (nonatomic, weak, readwrite, nullable) SVContext *context;
@property (nonatomic, assign, readwrite, getter=isRunning) BOOL running;

@end

@implementation SVSystemAudioInput
{
#if TARGET_OS_OSX
    AudioObjectID _tap;
    AudioObjectID _aggregateDevice;
    AudioDeviceIOProcID _ioProc;
    dispatch_queue_t _queue;
#endif
}

+ (BOOL)isSupported
{
#if TARGET_OS_OSX
    if (@available(macOS 14.2, *))
        return YES;
#endif
    return NO;
}

- (nullable instancetype)initWithContext:(SVContext *)context
{
    if (![[self class] isSupported])
        return nil;

    self = [super init];

    if (self == nil)
        return nil;

    _context = context;
#if TARGET_OS_OSX
    _tap = kAudioObjectUnknown;
    _aggregateDevice = kAudioObjectUnknown;
    _ioProc = nullptr;
#endif
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

#if TARGET_OS_OSX
    if (@available(macOS 14.2, *))
    {
        if ([self startTapWithError:error])
            return YES;

        [self stop];
        return NO;
    }
#endif

    if (error != nullptr)
        *error = SVMakeError(SVErrorArgument, @"system audio capture needs macOS 14.2 or later");
    return NO;
}

#if TARGET_OS_OSX
- (BOOL)startTapWithError:(NSError **)error API_AVAILABLE(macos(14.2))
{
    CATapDescription *description = [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:@[]];
    description.name = kAggregateDeviceName;
    description.privateTap = YES;

    OSStatus status = AudioHardwareCreateProcessTap(description, &_tap);

    if (status != noErr)
    {
        if (error != nullptr)
            *error = SVMakeOSStatusError(@"AudioHardwareCreateProcessTap", status);
        return NO;
    }

    AudioObjectPropertyAddress formatAddress = { kAudioTapPropertyFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioStreamBasicDescription tapDescription = {};
    UInt32 size = sizeof(tapDescription);
    status = AudioObjectGetPropertyData(_tap, &formatAddress, 0, nullptr, &size, &tapDescription);

    if (status != noErr)
    {
        if (error != nullptr)
            *error = SVMakeOSStatusError(@"kAudioTapPropertyFormat", status);
        return NO;
    }

    NSMutableDictionary *aggregate = [@{
        @kAudioAggregateDeviceNameKey: kAggregateDeviceName,
        @kAudioAggregateDeviceUIDKey: [NSUUID UUID].UUIDString,
        @kAudioAggregateDeviceIsPrivateKey: @YES,
        @kAudioAggregateDeviceIsStackedKey: @NO,
        @kAudioAggregateDeviceTapAutoStartKey: @YES,
        @kAudioAggregateDeviceTapListKey: @[ @{
            @kAudioSubTapUIDKey: description.UUID.UUIDString,
            @kAudioSubTapDriftCompensationKey: @YES
        } ]
    } mutableCopy];

    NSString *outputUID = SVDefaultOutputDeviceUID();

    if (outputUID != nil)
    {
        aggregate[@kAudioAggregateDeviceMainSubDeviceKey] = outputUID;
        aggregate[@kAudioAggregateDeviceSubDeviceListKey] = @[ @{ @kAudioSubDeviceUIDKey: outputUID } ];
    }

    status = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggregate, &_aggregateDevice);

    if (status != noErr)
    {
        if (error != nullptr)
            *error = SVMakeOSStatusError(@"AudioHardwareCreateAggregateDevice", status);
        return NO;
    }

    AVAudioFormat *tapFormat = [[AVAudioFormat alloc] initWithStreamDescription:&tapDescription];
    AVAudioFormat *targetFormat = SVMakeTargetAudioFormat();
    AVAudioConverter *converter = tapFormat != nil ? [[AVAudioConverter alloc] initFromFormat:tapFormat toFormat:targetFormat] : nil;

    if (converter == nil)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorIO, @"no converter from the system tap format to 44100 Hz stereo float");
        return NO;
    }

    __weak SVContext *weakContext = self.context;
    const double ratio = targetFormat.sampleRate / tapFormat.sampleRate;
    _queue = dispatch_queue_create("com.synesthesiavibe.system-audio", DISPATCH_QUEUE_SERIAL);

    status = AudioDeviceCreateIOProcIDWithBlock(&_ioProc, _aggregateDevice, _queue,
        ^(const AudioTimeStamp *now, const AudioBufferList *input, const AudioTimeStamp *inputTime, AudioBufferList *output, const AudioTimeStamp *outputTime) {
            SVContext *context = weakContext;

            if (context == nil || input == nullptr)
                return;

            AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:tapFormat bufferListNoCopy:input deallocator:nil];

            if (buffer == nil || buffer.frameLength == 0)
                return;

            SVConvertAndPushPCM(context, converter, targetFormat, buffer, ratio);
        });

    if (status != noErr)
    {
        if (error != nullptr)
            *error = SVMakeOSStatusError(@"AudioDeviceCreateIOProcIDWithBlock", status);
        return NO;
    }

    status = AudioDeviceStart(_aggregateDevice, _ioProc);

    if (status != noErr)
    {
        if (error != nullptr)
            *error = SVMakeOSStatusError(@"AudioDeviceStart", status);
        return NO;
    }

    self.running = YES;
    return YES;
}
#endif

- (void)stop
{
#if TARGET_OS_OSX
    if (_aggregateDevice != kAudioObjectUnknown && _ioProc != nullptr)
    {
        AudioDeviceStop(_aggregateDevice, _ioProc);
        AudioDeviceDestroyIOProcID(_aggregateDevice, _ioProc);
    }

    _ioProc = nullptr;

    if (_aggregateDevice != kAudioObjectUnknown)
        AudioHardwareDestroyAggregateDevice(_aggregateDevice);

    _aggregateDevice = kAudioObjectUnknown;

    if (@available(macOS 14.2, *))
    {
        if (_tap != kAudioObjectUnknown)
            AudioHardwareDestroyProcessTap(_tap);
    }

    _tap = kAudioObjectUnknown;
    _queue = nil;
#endif
    self.running = NO;
}

@end
