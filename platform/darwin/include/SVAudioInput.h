//
//  SVAudioInput.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

#import "SVContext.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SVAudioAuthorizationStatus) {
    SVAudioAuthorizationStatusUnknown = -1,
    SVAudioAuthorizationStatusNotDetermined = 0,
    SVAudioAuthorizationStatusRestricted = 1,
    SVAudioAuthorizationStatusDenied = 2,
    SVAudioAuthorizationStatusAuthorized = 3
};

typedef void (^SVAudioPCMHandler)(const float *interleaved, NSUInteger frames, NSInteger channels);

/* An input pushes its 44.1 kHz stereo PCM once into the process-wide ring, so one running input drives every SVView in the process and keeps doing so after its own context goes away; pcmHandler is called on the audio thread. */
@interface SVAudioInput : NSObject

@property (nonatomic, weak, readonly, nullable) SVContext *context;
@property (nonatomic, assign, readonly, getter=isRunning) BOOL running;
@property (atomic, assign) float inputGain;
@property (atomic, copy, nullable) SVAudioPCMHandler pcmHandler;
@property (class, nonatomic, assign, readonly) SVAudioAuthorizationStatus authorizationStatus;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithContext:(SVContext *)context NS_DESIGNATED_INITIALIZER;

+ (void)requestAccessWithCompletion:(void (^)(SVAudioAuthorizationStatus status))completion NS_SWIFT_NAME(requestAccess(completion:));

- (BOOL)startWithError:(NSError **)error NS_SWIFT_NAME(start());
- (void)stop;

@end

NS_ASSUME_NONNULL_END
