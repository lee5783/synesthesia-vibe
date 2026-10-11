//
//  VisualizerViewController.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <AppKit/AppKit.h>
@import SynesthesiaVibeCore;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, VisualizerAudioSource) {
    VisualizerAudioSourceWav,
    VisualizerAudioSourceMicrophone,
    VisualizerAudioSourceSystemAudio
};

@class VisualizerViewController;

@protocol VisualizerViewControllerDelegate <NSObject>

- (void)visualizerViewController:(VisualizerViewController *)controller didLoadPresetAtURL:(NSURL *)url;
- (void)visualizerViewController:(VisualizerViewController *)controller didFailWithError:(NSError *)error;

@end

@interface VisualizerViewController : NSViewController

@property (nonatomic, strong, readonly) SVView *visualizerView;
@property (nonatomic, weak, nullable) id<VisualizerViewControllerDelegate> delegate;
@property (nonatomic, assign, getter=isPaused) BOOL paused;
@property (nonatomic, copy, readonly, nullable) NSURL *presetURL;
@property (nonatomic, copy, readonly, nullable) NSURL *wavURL;
@property (nonatomic, assign, readonly) VisualizerAudioSource audioSource;
@property (nonatomic, assign, readonly) SVAudioLevels audioLevels;

- (void)loadPresetAtURL:(NSURL *)url;
- (BOOL)playWavAtURL:(NSURL *)url error:(NSError **)error;
- (BOOL)selectAudioSource:(VisualizerAudioSource)audioSource error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
