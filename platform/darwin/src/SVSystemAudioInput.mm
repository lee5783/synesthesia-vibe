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

#include <stdio.h>
#include <unistd.h>
#endif

#if TARGET_OS_OSX
namespace
{
    NSString* const kAggregateDeviceName = @"SynesthesiaVibe System Audio";
    constexpr AVAudioFrameCount kMaximumTapFrames = 8192;
    constexpr useconds_t kAccessProbeMicroseconds = 200000;
    constexpr AudioObjectPropertySelector kObservedSelectors[] = {
        kAudioHardwarePropertyDefaultSystemOutputDevice,
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioHardwarePropertyProcessObjectList
    };

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

    AudioObjectID SVCurrentProcessAudioObject()
    {
        const AudioObjectPropertyAddress address = { kAudioHardwarePropertyTranslatePIDToProcessObject, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        const pid_t pid = getpid();
        AudioObjectID process = kAudioObjectUnknown;
        UInt32 size = sizeof(process);

        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, sizeof(pid), &pid, &size, &process) != noErr)
            return kAudioObjectUnknown;

        return process;
    }

    API_AVAILABLE(macos(14.2))
    CATapDescription* SVMakeTapDescription(AudioObjectID excludedProcess)
    {
        NSArray<NSNumber*>* excluded = excludedProcess != kAudioObjectUnknown ? @[ @(excludedProcess) ] : @[];
        CATapDescription* description = [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:excluded];
        description.name = kAggregateDeviceName;
        description.privateTap = YES;
        return description;
    }

    API_AVAILABLE(macos(14.2))
    BOOL SVCreateTapDevice(CATapDescription* description, AudioObjectID* tap, AudioObjectID* aggregateDevice, AudioStreamBasicDescription* tapFormat, NSError** error)
    {
        OSStatus status = AudioHardwareCreateProcessTap(description, tap);

        if (status != noErr)
        {
            if (error != nullptr)
                *error = SVMakeOSStatusError(@"AudioHardwareCreateProcessTap", status);
            return NO;
        }

        AudioObjectPropertyAddress formatAddress = { kAudioTapPropertyFormat, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        UInt32 size = sizeof(*tapFormat);
        status = AudioObjectGetPropertyData(*tap, &formatAddress, 0, nullptr, &size, tapFormat);

        if (status != noErr)
        {
            if (error != nullptr)
                *error = SVMakeOSStatusError(@"kAudioTapPropertyFormat", status);
            return NO;
        }

        NSMutableDictionary* aggregate = [@{
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

        NSString* outputUID = SVDefaultOutputDeviceUID();

        if (outputUID != nil)
        {
            aggregate[@kAudioAggregateDeviceMainSubDeviceKey] = outputUID;
            aggregate[@kAudioAggregateDeviceSubDeviceListKey] = @[ @{ @kAudioSubDeviceUIDKey: outputUID } ];
        }

        status = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)aggregate, aggregateDevice);

        if (status != noErr)
        {
            if (error != nullptr)
                *error = SVMakeOSStatusError(@"AudioHardwareCreateAggregateDevice", status);
            return NO;
        }

        return YES;
    }

    API_AVAILABLE(macos(14.2))
    void SVDestroyTapDevice(AudioObjectID* tap, AudioObjectID* aggregateDevice, AudioDeviceIOProcID* ioProc)
    {
        if (*aggregateDevice != kAudioObjectUnknown && *ioProc != nullptr)
        {
            AudioDeviceStop(*aggregateDevice, *ioProc);
            AudioDeviceDestroyIOProcID(*aggregateDevice, *ioProc);
        }

        *ioProc = nullptr;

        if (*aggregateDevice != kAudioObjectUnknown)
            AudioHardwareDestroyAggregateDevice(*aggregateDevice);

        *aggregateDevice = kAudioObjectUnknown;

        if (*tap != kAudioObjectUnknown)
            AudioHardwareDestroyProcessTap(*tap);

        *tap = kAudioObjectUnknown;
    }

    API_AVAILABLE(macos(14.2))
    void SVProbeSystemTapAccess()
    {
        AudioObjectID tap = kAudioObjectUnknown;
        AudioObjectID aggregateDevice = kAudioObjectUnknown;
        AudioDeviceIOProcID ioProc = nullptr;
        AudioStreamBasicDescription tapFormat = {};
        NSError* error = nil;
        dispatch_queue_t queue = dispatch_queue_create("com.synesthesiavibe.system-audio-probe", DISPATCH_QUEUE_SERIAL);

        if (SVCreateTapDevice(SVMakeTapDescription(kAudioObjectUnknown), &tap, &aggregateDevice, &tapFormat, &error))
        {
            OSStatus status = AudioDeviceCreateIOProcIDWithBlock(&ioProc, aggregateDevice, queue,
                ^(const AudioTimeStamp*, const AudioBufferList*, const AudioTimeStamp*, AudioBufferList*, const AudioTimeStamp*) {});

            if (status == noErr)
                status = AudioDeviceStart(aggregateDevice, ioProc);

            if (status == noErr)
                usleep(kAccessProbeMicroseconds);
            else
                error = SVMakeOSStatusError(@"AudioDeviceStart", status);
        }

        fprintf(stderr, "SVSystemAudioInput: access probe %s\n", error != nil ? error.localizedDescription.UTF8String : "started");
        SVDestroyTapDevice(&tap, &aggregateDevice, &ioProc);
    }
}
#endif

@interface SVSystemAudioInput ()

@property (nonatomic, weak, readwrite, nullable) SVContext *context;
@property (nonatomic, assign, readwrite, getter=isRunning) BOOL running;

@end

@implementation SVSystemAudioInput
{
    SVAudioPCMSink *_sink;
#if TARGET_OS_OSX
    AudioObjectID _tap;
    AudioObjectID _aggregateDevice;
    AudioDeviceIOProcID _ioProc;
    AudioObjectID _excludedProcess;
    id _tapDescription;
    dispatch_queue_t _queue;
    AudioObjectPropertyListenerBlock _hardwareListener;
    BOOL _restartPending;
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

+ (SVAudioAuthorizationStatus)authorizationStatus
{
    return SVAudioAuthorizationStatusUnknown;
}

+ (void)requestAccessWithCompletion:(void (^)(SVAudioAuthorizationStatus status))completion
{
    void (^handler)(SVAudioAuthorizationStatus) = [completion copy];

#if TARGET_OS_OSX
    if (@available(macOS 14.2, *))
    {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            SVProbeSystemTapAccess();
            dispatch_async(dispatch_get_main_queue(), ^{
                if (handler != nil)
                    handler([SVSystemAudioInput authorizationStatus]);
            });
        });
        return;
    }
#endif

    dispatch_async(dispatch_get_main_queue(), ^{
        if (handler != nil)
            handler([SVSystemAudioInput authorizationStatus]);
    });
}

- (nullable instancetype)initWithContext:(SVContext *)context
{
    if (![[self class] isSupported])
        return nil;

    self = [super init];

    if (self == nil)
        return nil;

    _context = context;
    _sink = [[SVAudioPCMSink alloc] initWithContext:context];
    _excludesCurrentProcessAudio = YES;
#if TARGET_OS_OSX
    _tap = kAudioObjectUnknown;
    _aggregateDevice = kAudioObjectUnknown;
    _ioProc = nullptr;
    _excludedProcess = kAudioObjectUnknown;
#endif
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
    if (self.running)
        return YES;

#if TARGET_OS_OSX
    if (@available(macOS 14.2, *))
    {
        if (![self startTapWithError:error])
        {
            [self teardownTap];
            return NO;
        }

        [self startObservingHardware];
        self.running = YES;
        return YES;
    }
#endif

    if (error != nullptr)
        *error = SVMakeError(SVErrorArgument, @"system audio capture needs macOS 14.2 or later");
    return NO;
}

#if TARGET_OS_OSX
- (BOOL)startTapWithError:(NSError **)error API_AVAILABLE(macos(14.2))
{
    const AudioObjectID excludedProcess = _excludesCurrentProcessAudio ? SVCurrentProcessAudioObject() : kAudioObjectUnknown;
    CATapDescription *description = SVMakeTapDescription(excludedProcess);
    AudioStreamBasicDescription tapDescription = {};

    if (!SVCreateTapDevice(description, &_tap, &_aggregateDevice, &tapDescription, error))
        return NO;

    _tapDescription = description;
    _excludedProcess = excludedProcess;

    AVAudioFormat *tapFormat = [[AVAudioFormat alloc] initWithStreamDescription:&tapDescription];
    SVAudioConversion *conversion = tapFormat != nil ? [[SVAudioConversion alloc] initWithInputFormat:tapFormat maximumInputFrames:kMaximumTapFrames] : nil;

    if (conversion == nil)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorIO, @"no converter from the system tap format to 44100 Hz stereo float");
        return NO;
    }

    SVAudioPCMSink *sink = _sink;
    _queue = dispatch_queue_create("com.synesthesiavibe.system-audio", DISPATCH_QUEUE_SERIAL);

    OSStatus status = AudioDeviceCreateIOProcIDWithBlock(&_ioProc, _aggregateDevice, _queue,
        ^(const AudioTimeStamp *now, const AudioBufferList *input, const AudioTimeStamp *inputTime, AudioBufferList *output, const AudioTimeStamp *outputTime) {
            if (input == nullptr)
                return;

            @autoreleasepool
            {
                [conversion convertBufferList:input intoSink:sink];
            }
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

    fprintf(stderr, "SVSystemAudioInput: tap started %.0f Hz %u channels excluded-process=%u\n", tapFormat.sampleRate,
            static_cast<unsigned>(tapFormat.channelCount), static_cast<unsigned>(excludedProcess));
    return YES;
}

- (void)teardownTap
{
    if (@available(macOS 14.2, *))
        SVDestroyTapDevice(&_tap, &_aggregateDevice, &_ioProc);

    _tapDescription = nil;
    _excludedProcess = kAudioObjectUnknown;
    _queue = nil;
}

- (void)restartTapWithReason:(NSString *)reason API_AVAILABLE(macos(14.2))
{
    _restartPending = NO;

    if (!self.running)
        return;

    [self teardownTap];

    NSError *error = nil;
    const BOOL restarted = [self startTapWithError:&error];

    if (!restarted)
        [self teardownTap];

    fprintf(stderr, "SVSystemAudioInput: restart reason=%s %s\n", reason.UTF8String, restarted ? "ok" : (error.localizedDescription.UTF8String ?: "failed"));
}

- (void)scheduleRestartWithReason:(NSString *)reason API_AVAILABLE(macos(14.2))
{
    if (_restartPending)
        return;

    _restartPending = YES;
    __weak SVSystemAudioInput *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf restartTapWithReason:reason];
    });
}

- (void)updateCurrentProcessExclusion API_AVAILABLE(macos(14.2))
{
    if (!self.running || !_excludesCurrentProcessAudio || _excludedProcess != kAudioObjectUnknown || _tap == kAudioObjectUnknown)
        return;

    const AudioObjectID process = SVCurrentProcessAudioObject();

    if (process == kAudioObjectUnknown)
        return;

    CATapDescription *description = static_cast<CATapDescription *>(_tapDescription);
    description.processes = @[ @(process) ];

    const AudioObjectPropertyAddress address = { kAudioTapPropertyDescription, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    CFTypeRef value = (__bridge CFTypeRef)description;
    const OSStatus status = AudioObjectSetPropertyData(_tap, &address, 0, nullptr, sizeof(value), &value);

    if (status != noErr)
    {
        fprintf(stderr, "SVSystemAudioInput: exclude current process failed (OSStatus %d)\n", static_cast<int>(status));
        [self scheduleRestartWithReason:@"current process audio object appeared"];
        return;
    }

    _excludedProcess = process;
    fprintf(stderr, "SVSystemAudioInput: excluded current process object %u\n", static_cast<unsigned>(process));
}

- (void)startObservingHardware API_AVAILABLE(macos(14.2))
{
    if (_hardwareListener != nil)
        return;

    __weak SVSystemAudioInput *weakSelf = self;
    AudioObjectPropertyListenerBlock listener = ^(UInt32 addressCount, const AudioObjectPropertyAddress *addresses) {
        SVSystemAudioInput *input = weakSelf;

        if (input == nil)
            return;

        BOOL outputDeviceChanged = NO;

        for (UInt32 index = 0; index < addressCount; ++index)
        {
            if (addresses[index].mSelector != kAudioHardwarePropertyProcessObjectList)
                outputDeviceChanged = YES;
        }

        if (outputDeviceChanged)
            [input scheduleRestartWithReason:@"default output device change"];
        else
            [input updateCurrentProcessExclusion];
    };

    for (AudioObjectPropertySelector selector : kObservedSelectors)
    {
        const AudioObjectPropertyAddress address = { selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioObjectAddPropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), listener);
    }

    _hardwareListener = listener;
    [self updateCurrentProcessExclusion];
}

- (void)stopObservingHardware
{
    if (_hardwareListener == nil)
        return;

    for (AudioObjectPropertySelector selector : kObservedSelectors)
    {
        const AudioObjectPropertyAddress address = { selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
        AudioObjectRemovePropertyListenerBlock(kAudioObjectSystemObject, &address, dispatch_get_main_queue(), _hardwareListener);
    }

    _hardwareListener = nil;
    _restartPending = NO;
}
#endif

- (void)stop
{
#if TARGET_OS_OSX
    [self stopObservingHardware];
    [self teardownTap];
#endif
    self.running = NO;
}

@end
