//
//  SVSystemAudioInput.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

#import "SVAudioInput.h"
#import "SVContext.h"

NS_ASSUME_NONNULL_BEGIN

/* Like SVAudioInput, a system tap pushes once into the process-wide ring and drives every SVView; macOS reports no tap permission, so authorizationStatus is Unknown and a denied tap may deliver silence. */
@interface SVSystemAudioInput : NSObject

@property (nonatomic, weak, readonly, nullable) SVContext *context;
@property (nonatomic, assign, readonly, getter=isRunning) BOOL running;
@property (atomic, assign) float inputGain;
@property (atomic, copy, nullable) SVAudioPCMHandler pcmHandler;
@property (nonatomic, assign) BOOL excludesCurrentProcessAudio;
@property (class, nonatomic, assign, readonly, getter=isSupported) BOOL supported;
@property (class, nonatomic, assign, readonly) SVAudioAuthorizationStatus authorizationStatus;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (nullable instancetype)initWithContext:(SVContext *)context NS_DESIGNATED_INITIALIZER;

+ (void)requestAccessWithCompletion:(void (^)(SVAudioAuthorizationStatus status))completion NS_SWIFT_NAME(requestAccess(completion:));

- (BOOL)startWithError:(NSError **)error NS_SWIFT_NAME(start());
- (void)stop;

@end

NS_ASSUME_NONNULL_END
