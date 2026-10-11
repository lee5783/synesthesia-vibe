//
//  SVCaptureSession.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

#import "SVContextConfiguration.h"

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT const NSInteger SVCaptureWidth;
FOUNDATION_EXPORT const NSInteger SVCaptureHeight;
FOUNDATION_EXPORT const uint32_t SVCaptureRngSeed;

/* -runWithError: creates its own SVContext and offscreen framebuffer: call it on the render thread with a GL context current. */
@interface SVCaptureSession : NSObject

@property (nonatomic, copy, readonly) SVContextConfiguration *configuration;
@property (nonatomic, copy, readonly) NSURL *presetURL;
@property (nonatomic, copy, readonly, nullable) NSURL *wavURL;
@property (nonatomic, copy, readonly) NSArray<NSNumber *> *frames;
@property (nonatomic, copy, readonly) NSURL *outputDirectoryURL;
@property (nonatomic, assign) BOOL writesFrameState;
@property (nonatomic, copy, readonly) NSArray<NSURL *> *writtenFileURLs;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithConfiguration:(SVContextConfiguration *)configuration
                            presetURL:(NSURL *)presetURL
                               wavURL:(nullable NSURL *)wavURL
                               frames:(NSArray<NSNumber *> *)frames
                   outputDirectoryURL:(NSURL *)outputDirectoryURL NS_DESIGNATED_INITIALIZER;

- (BOOL)runWithError:(NSError **)error NS_SWIFT_NAME(run());

@end

NS_ASSUME_NONNULL_END
