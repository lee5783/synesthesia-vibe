//
//  InspectorViewController.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@class InspectorViewController;

@protocol InspectorViewControllerDelegate <NSObject>

- (void)inspectorViewController:(InspectorViewController *)controller didSetAutoChange:(BOOL)on;
- (void)inspectorViewController:(InspectorViewController *)controller didSetFrameRate:(float)frameRate;
- (void)inspectorViewController:(InspectorViewController *)controller didSelectAudioSourceIndex:(NSInteger)index;
- (void)inspectorViewController:(InspectorViewController *)controller didSetDebugOverlay:(BOOL)on;

@end

@interface InspectorViewController : NSViewController

@property (nonatomic, weak, nullable) id<InspectorViewControllerDelegate> delegate;
@property (nonatomic, assign) BOOL systemAudioAvailable;
@property (nonatomic, copy, nullable) NSString *audioReason;

- (void)setAutoChangeOn:(BOOL)on interval:(double)interval remaining:(double)remaining;
- (void)setFrameRate:(float)frameRate;
- (void)setAudioSourceIndex:(NSInteger)index;
- (void)setDebugOverlayOn:(BOOL)on;

@end

NS_ASSUME_NONNULL_END
