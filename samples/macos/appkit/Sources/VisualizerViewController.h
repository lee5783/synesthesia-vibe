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
    VisualizerAudioSourceNone,
    VisualizerAudioSourceWav,
    VisualizerAudioSourceMicrophone,
    VisualizerAudioSourceSystemAudio
};

@class VisualizerViewController;

@protocol VisualizerViewControllerDelegate <NSObject>

- (void)visualizerViewController:(VisualizerViewController *)controller didLoadPresetAtURL:(NSURL *)url;
- (void)visualizerViewController:(VisualizerViewController *)controller didFailWithError:(NSError *)error;
- (void)visualizerViewController:(VisualizerViewController *)controller didUpdateDebugInfo:(NSDictionary<NSString *, id> *)info;

@end

@interface VisualizerViewController : NSViewController

@property (nonatomic, strong, readonly) SVView *visualizerView;
@property (nonatomic, weak, nullable) id<VisualizerViewControllerDelegate> delegate;
@property (nonatomic, assign, getter=isPaused) BOOL paused;
@property (nonatomic, copy, readonly, nullable) NSURL *presetURL;
@property (nonatomic, copy, readonly, nullable) NSURL *wavURL;
@property (nonatomic, assign, readonly) VisualizerAudioSource audioSource;
@property (nonatomic, assign, readonly) SVAudioLevels audioLevels;
@property (nonatomic, assign, readonly) double audioSampleRate;
@property (nonatomic, copy, readonly, nullable) NSString *glVersionString;
@property (nonatomic, copy, readonly, nullable) NSDictionary<NSString *, id> *lastDebugInfo;
@property (nonatomic, assign) float preferredFramesPerSecond;
@property (nonatomic, assign) CGFloat renderScale;
@property (nonatomic, assign) float playbackRate;
@property (nonatomic, copy, nullable) void (^debugInfoHandler)(NSDictionary<NSString *, id> *info);
@property (nonatomic, assign) float debugInterval;
@property (nonatomic, assign) BOOL debugEnabled;
@property (nonatomic, copy) SVTransitionConfiguration *transitionConfiguration;
@property (nonatomic, assign) NSInteger blendPattern;
@property (nonatomic, assign) float blendSeconds;
@property (nonatomic, copy, nullable) void (^presetRequestHandler)(SVPresetRequest *request);

- (void)loadPresetAtURL:(NSURL *)url;
- (void)loadPresetAtURL:(NSURL *)url blendSeconds:(float)blendSeconds;
- (void)saveSnapshotToURL:(NSURL *)url completion:(void (^)(NSError *_Nullable error))completion;
- (BOOL)playWavAtURL:(NSURL *)url error:(NSError **)error;
- (BOOL)selectAudioSource:(VisualizerAudioSource)audioSource error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
