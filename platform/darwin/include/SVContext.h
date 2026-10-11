//
//  SVContext.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>
#import <stdint.h>

#import "SVContextConfiguration.h"

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSErrorDomain const SVErrorDomain;

typedef NS_ERROR_ENUM(SVErrorDomain, SVErrorCode) {
    SVErrorIO = 1,
    SVErrorParse = 2,
    SVErrorShader = 3,
    SVErrorGL = 4,
    SVErrorArgument = 5,
    SVErrorRendererUnavailable = 100
};

typedef struct SVAudioLevels {
    float bass;
    float mid;
    float treble;
} SVAudioLevels;

/* Every method except the PCM pushes, audioLevels and lastErrorMessage touches GL: call it on the render thread with the GL context current. */
@interface SVContext : NSObject

@property (nonatomic, copy, readonly) SVContextConfiguration *configuration;
@property (nonatomic, assign, readonly, getter=isPaused) BOOL paused;
@property (nonatomic, assign, readonly) SVAudioLevels audioLevels;
@property (nonatomic, copy, readonly, nullable) NSString *lastErrorMessage;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (nullable instancetype)initWithConfiguration:(SVContextConfiguration *)configuration
                                         error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (BOOL)loadPresetAtURL:(NSURL *)url
           blendSeconds:(float)blendSeconds
                  error:(NSError **)error NS_SWIFT_NAME(loadPreset(at:blendSeconds:));

- (BOOL)loadPresetText:(NSString *)text
          blendSeconds:(float)blendSeconds
                 error:(NSError **)error NS_SWIFT_NAME(loadPreset(text:blendSeconds:));

- (void)pushPCMFloat:(const float *)interleaved
              frames:(NSUInteger)frames
            channels:(NSInteger)channels NS_SWIFT_NAME(pushPCM(float:frames:channels:));

- (void)pushPCMInt16:(const int16_t *)interleaved
              frames:(NSUInteger)frames
            channels:(NSInteger)channels NS_SWIFT_NAME(pushPCM(int16:frames:channels:));

- (void)renderFrameWithDeltaTime:(NSTimeInterval)deltaTime
               targetFramebuffer:(uint32_t)targetFramebuffer NS_SWIFT_NAME(renderFrame(deltaTime:targetFramebuffer:));

- (void)resizeToWidth:(NSInteger)width height:(NSInteger)height NS_SWIFT_NAME(resize(width:height:));

- (void)pause;
- (void)resume;

- (BOOL)dumpFrameStateToURL:(NSURL *)url error:(NSError **)error NS_SWIFT_NAME(dumpFrameState(to:));

- (void)invalidate;

@end

NS_ASSUME_NONNULL_END
