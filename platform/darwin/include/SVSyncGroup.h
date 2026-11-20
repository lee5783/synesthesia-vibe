//
//  SVSyncGroup.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

#import "SVContext.h"
#import "SVContextConfiguration.h"
#import "SVTransitionConfiguration.h"

NS_ASSUME_NONNULL_BEGIN

@interface SVSyncGroup : NSObject

@property (nonatomic, copy, readonly) SVContextConfiguration *configuration;
@property (nonatomic, strong, readonly) SVContext *context;
@property (nonatomic, assign, readonly) CGSize renderSize;
@property (nonatomic, assign, readonly, getter=isPaused) BOOL paused;
@property (nonatomic, assign, readonly) NSUInteger memberCount;
@property (nonatomic, assign) CGSize fixedRenderSize;
@property (nonatomic, assign) CGFloat renderScale;
@property (nonatomic, assign) float preferredFramesPerSecond;
@property (nonatomic, assign) float playbackRate;
@property (nonatomic, copy, nullable) void (^debugInfoHandler)(NSDictionary<NSString *, id> *info);
@property (nonatomic, assign) float debugInterval;
@property (nonatomic, assign) BOOL debugEnabled;
@property (nonatomic, copy) SVTransitionConfiguration *transitionConfiguration;
@property (nonatomic, assign) NSInteger blendPattern;
@property (nonatomic, copy, nullable) void (^presetRequestHandler)(SVPresetRequest *request);

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (nullable instancetype)initWithConfiguration:(SVContextConfiguration *)configuration
                                         error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (void)performWithContextCurrent:(void (NS_NOESCAPE ^)(void))block;

@end

NS_ASSUME_NONNULL_END
