//
//  VisualizerContentViewController.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import <AppKit/AppKit.h>
@import SynesthesiaVibeCore;

NS_ASSUME_NONNULL_BEGIN

@class VisualizerViewController;
@class VisualizerContentViewController;

@protocol VisualizerContentViewControllerDelegate <NSObject>

- (void)visualizerContentViewControllerDidToggleDebugOverlay:(VisualizerContentViewController *)controller;
- (void)visualizerContentViewControllerDidToggleFullScreen:(VisualizerContentViewController *)controller;

@end

@interface VisualizerContentViewController : NSViewController

@property (nonatomic, weak, nullable) id<VisualizerContentViewControllerDelegate> delegate;
@property (nonatomic, strong, readonly) VisualizerViewController *visualizerViewController;
@property (nonatomic, assign, getter=isFullScreen) BOOL fullScreen;

- (instancetype)initWithVisualizerViewController:(VisualizerViewController *)visualizerViewController;

- (void)updateDebugOverlayVisible:(BOOL)visible on:(BOOL)on;
- (void)updateDebugOverlayWithInfo:(nullable NSDictionary<NSString *, id> *)info
                               cap:(float)cap
                          fpsText:(NSString *)fpsText
                          fpsHealthy:(BOOL)fpsHealthy
                     millisecondsText:(NSString *)millisecondsText
                            source:(NSString *)source
                        sampleRate:(double)sampleRate
                          backend:(nullable NSString *)backend
                       renderScale:(CGFloat)renderScale
                              autoChangeText:(NSString *)autoChangeText
                              transitionText:(NSString *)transitionText
                         lastError:(nullable NSString *)lastError
                            levels:(SVAudioLevels)levels;
- (void)showMessage:(nullable NSString *)message isError:(BOOL)isError;

@end

NS_ASSUME_NONNULL_END
