//
//  SVView.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

#import "SVContext.h"
#import "SVContextConfiguration.h"

NS_ASSUME_NONNULL_BEGIN

@class SVView;

@protocol SVViewDelegate <NSObject>

@optional
- (void)view:(SVView *)view didFailWithError:(NSError *)error;
- (void)view:(SVView *)view didCreateContext:(SVContext *)context;
- (void)view:(SVView *)view willRenderFrameWithDeltaTime:(NSTimeInterval)deltaTime;

@end

#if TARGET_OS_OSX
@interface SVView : NSView
#else
@interface SVView : UIView
#endif

@property (nonatomic, copy, null_resettable) SVContextConfiguration *configuration;
@property (nonatomic, strong, readonly, nullable) SVContext *context;
@property (nonatomic, assign, getter=isPaused) BOOL paused;
@property (nonatomic, weak, nullable) id<SVViewDelegate> delegate;
@property (nonatomic, assign, readonly) CGSize drawableSize;

@end

NS_ASSUME_NONNULL_END
