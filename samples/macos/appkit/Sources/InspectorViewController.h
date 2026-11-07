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
- (void)inspectorViewController:(InspectorViewController *)controller didSetAutoChangeInterval:(double)interval;
- (void)inspectorViewController:(InspectorViewController *)controller didSelectBlendPattern:(NSInteger)pattern;
- (void)inspectorViewController:(InspectorViewController *)controller didSetBlendSeconds:(float)seconds;
- (void)inspectorViewController:(InspectorViewController *)controller didSetHardCuts:(BOOL)on;
- (void)inspectorViewController:(InspectorViewController *)controller didSetFrameRate:(float)frameRate;
- (void)inspectorViewController:(InspectorViewController *)controller didSelectAudioSourceIndex:(NSInteger)index;
- (void)inspectorViewController:(InspectorViewController *)controller didSetDebugOverlay:(BOOL)on;

@end

@interface InspectorViewController : NSViewController

@property (nonatomic, weak, nullable) id<InspectorViewControllerDelegate> delegate;
@property (nonatomic, assign) BOOL systemAudioAvailable;
@property (nonatomic, copy, nullable) NSString *audioReason;

- (void)setAutoChangeOn:(BOOL)on interval:(double)interval;
- (void)setBlendPattern:(NSInteger)pattern blendSeconds:(float)seconds hardCutsOn:(BOOL)hardCutsOn;
- (void)setFrameRate:(float)frameRate;
- (void)setAudioSourceIndex:(NSInteger)index;
- (void)setDebugOverlayOn:(BOOL)on;

@end

NS_ASSUME_NONNULL_END
