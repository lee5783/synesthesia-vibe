//
//  VisualizerViewController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "VisualizerViewController.h"

#import "SampleBundle.h"

static const NSTimeInterval VisualizerMaximumFeedSeconds = 0.25;
static const NSTimeInterval VisualizerFrameRateLogInterval = 1.0;
static const NSTimeInterval VisualizerLaunchSnapshotDelay = 3.0;

static void VisualizerLog(NSString *message)
{
    fprintf(stderr, "AppKitSample: %s\n", message.UTF8String);
}

static NSString *_Nullable VisualizerLaunchValue(NSArray<NSString *> *arguments, NSString *name)
{
    NSUInteger index = [arguments indexOfObject:name];
    if (index == NSNotFound || index + 1 >= arguments.count)
        return nil;
    return arguments[index + 1];
}

static float VisualizerLaunchMaxFps(NSArray<NSString *> *arguments)
{
    return VisualizerLaunchValue(arguments, @"--max-fps").floatValue;
}

static float VisualizerLaunchFloat(NSArray<NSString *> *arguments, NSString *name, float fallback)
{
    NSString *value = VisualizerLaunchValue(arguments, name);
    return value != nil ? value.floatValue : fallback;
}

static void VisualizerLogValidation(NSString *_Nullable path)
{
    if (path == nil)
        return;

    NSError *error = nil;
    if ([SVContext validatePresetAtURL:[NSURL fileURLWithPath:path] error:&error])
        VisualizerLog([NSString stringWithFormat:@"validate \"%@\" ok", path]);
    else
        VisualizerLog([NSString stringWithFormat:@"validate \"%@\" failed code=%ld message=%@", path, (long)error.code, error.localizedDescription]);
}

static CGFloat VisualizerLaunchRenderScale(NSArray<NSString *> *arguments)
{
    NSString *value = VisualizerLaunchValue(arguments, @"--render-scale");
    return value != nil ? (CGFloat)value.doubleValue : 1.0;
}

static NSString *VisualizerAudioSourceName(VisualizerAudioSource audioSource)
{
    switch (audioSource) {
    case VisualizerAudioSourceWav:
        return @"wav";
    case VisualizerAudioSourceMicrophone:
        return @"microphone";
    case VisualizerAudioSourceSystemAudio:
        return @"system-audio";
    }
    return @"unknown";
}

@interface VisualizerViewController () <SVViewDelegate>

@property (nonatomic, strong, readwrite) SVView *visualizerView;
@property (nonatomic, copy, readwrite, nullable) NSURL *presetURL;
@property (nonatomic, copy, readwrite, nullable) NSURL *wavURL;
@property (nonatomic, assign, readwrite) VisualizerAudioSource audioSource;
@property (nonatomic, copy, nullable) NSURL *pendingPresetURL;
@property (nonatomic, strong, nullable) SVWavFeeder *wavFeeder;
@property (nonatomic, assign) double wavFrameRemainder;
@property (nonatomic, strong, nullable) SVAudioInput *microphoneInput;
@property (nonatomic, strong, nullable) SVSystemAudioInput *systemAudioInput;
@property (nonatomic, assign) BOOL logsFrameRate;
@property (nonatomic, assign) NSUInteger renderedFrameCount;
@property (nonatomic, assign) NSTimeInterval frameRateWindowStart;
@property (nonatomic, copy, nullable) NSString *launchSnapshotPath;

@end

@implementation VisualizerViewController

- (void)loadView
{
    SVContextConfiguration *configuration = [[SVContextConfiguration alloc] init];
    configuration.resourceURL = SampleBundle.resourceURL;
    configuration.cacheURL = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
    configuration.meshX = 64;
    configuration.meshY = 48;
    configuration.fpsHint = 60.0f;
    NSArray<NSString *> *arguments = NSProcessInfo.processInfo.arguments;
    configuration.maxFps = VisualizerLaunchMaxFps(arguments);
    configuration.maxLuminance = VisualizerLaunchFloat(arguments, @"--max-luminance", 0.0f);
    configuration.flashLimit = VisualizerLaunchFloat(arguments, @"--flash-limit", 0.0f);
    VisualizerLogValidation(VisualizerLaunchValue(arguments, @"--validate"));
    self.logsFrameRate = [arguments containsObject:@"--log-frame-rate"];
    self.launchSnapshotPath = VisualizerLaunchValue(arguments, @"--snapshot");

    SVView *visualizerView = [[SVView alloc] initWithFrame:NSMakeRect(0, 0, 838, 441)];
    visualizerView.configuration = configuration;
    visualizerView.renderScale = VisualizerLaunchRenderScale(arguments);
    visualizerView.playbackRate = VisualizerLaunchFloat(arguments, @"--playback-rate", 1.0f);
    visualizerView.slowMotion = [VisualizerLaunchValue(arguments, @"--slow-motion") isEqualToString:@"fractional"] ? SVSlowMotionFractional : SVSlowMotionInterpolate;
    visualizerView.delegate = self;
    self.visualizerView = visualizerView;
    self.view = visualizerView;
}

- (BOOL)isPaused
{
    return self.visualizerView.paused;
}

- (void)setPaused:(BOOL)paused
{
    VisualizerLog([NSString stringWithFormat:@"paused=%d", paused]);
    self.visualizerView.paused = paused;
}

- (float)preferredFramesPerSecond
{
    return self.visualizerView.preferredFramesPerSecond;
}

- (void)setPreferredFramesPerSecond:(float)preferredFramesPerSecond
{
    VisualizerLog([NSString stringWithFormat:@"preferred fps=%.0f", preferredFramesPerSecond]);
    self.visualizerView.preferredFramesPerSecond = preferredFramesPerSecond;
    self.renderedFrameCount = 0;
    self.frameRateWindowStart = 0.0;
}

- (float)playbackRate
{
    return self.visualizerView.playbackRate;
}

- (void)setPlaybackRate:(float)playbackRate
{
    VisualizerLog([NSString stringWithFormat:@"playback rate=%.3f", playbackRate]);
    self.visualizerView.playbackRate = playbackRate;
}

- (SVSlowMotion)slowMotion
{
    return self.visualizerView.slowMotion;
}

- (void)setSlowMotion:(SVSlowMotion)slowMotion
{
    VisualizerLog([NSString stringWithFormat:@"slow motion=%ld", (long)slowMotion]);
    self.visualizerView.slowMotion = slowMotion;
}

- (CGFloat)renderScale
{
    return self.visualizerView.renderScale;
}

- (void)setRenderScale:(CGFloat)renderScale
{
    VisualizerLog([NSString stringWithFormat:@"render scale=%.2f", renderScale]);
    self.visualizerView.renderScale = renderScale;
}

- (void)saveSnapshotToURL:(NSURL *)url completion:(void (^)(NSError *_Nullable error))completion
{
    CGSize drawableSize = self.visualizerView.drawableSize;
    [self.visualizerView snapshotWithCompletion:^(CGImageRef image) {
        NSError *error = nil;
        NSData *png = nil;
        if (image != NULL)
            png = [[[NSBitmapImageRep alloc] initWithCGImage:image] representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        if (png == nil || ![png writeToURL:url options:NSDataWritingAtomic error:&error]) {
            if (error == nil)
                error = [NSError errorWithDomain:SVErrorDomain code:SVErrorIO userInfo:@{ NSLocalizedDescriptionKey: @"snapshot unavailable" }];
            VisualizerLog([NSString stringWithFormat:@"snapshot failed: %@", error.localizedDescription]);
        } else {
            VisualizerLog([NSString stringWithFormat:@"snapshot %zux%zu drawable=%.0fx%.0f \"%@\"", CGImageGetWidth(image), CGImageGetHeight(image),
                           drawableSize.width, drawableSize.height, url.path]);
        }
        completion(error);
    }];
}

- (void)scheduleLaunchSnapshot
{
    NSString *path = self.launchSnapshotPath;
    if (path == nil)
        return;

    self.launchSnapshotPath = nil;
    __weak VisualizerViewController *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(VisualizerLaunchSnapshotDelay * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [weakSelf saveSnapshotToURL:[NSURL fileURLWithPath:path] completion:^(NSError *error) {
            weakSelf.paused = YES;
        }];
    });
}

- (void)logFrameRateIfNeeded
{
    if (!self.logsFrameRate)
        return;

    NSTimeInterval now = NSProcessInfo.processInfo.systemUptime;
    if (self.frameRateWindowStart <= 0.0) {
        self.frameRateWindowStart = now;
        self.renderedFrameCount = 0;
        return;
    }

    self.renderedFrameCount += 1;
    NSTimeInterval elapsed = now - self.frameRateWindowStart;
    if (elapsed < VisualizerFrameRateLogInterval)
        return;

    VisualizerLog([NSString stringWithFormat:@"rendered fps=%.1f preferred=%.0f drawable=%.0fx%.0f", self.renderedFrameCount / elapsed,
                   self.visualizerView.preferredFramesPerSecond, self.visualizerView.drawableSize.width, self.visualizerView.drawableSize.height]);
    self.frameRateWindowStart = now;
    self.renderedFrameCount = 0;
}

- (SVAudioLevels)audioLevels
{
    SVContext *context = self.visualizerView.context;
    return context != nil ? context.audioLevels : (SVAudioLevels){ 0.0f, 0.0f, 0.0f };
}

- (void)loadPresetAtURL:(NSURL *)url
{
    VisualizerLog([NSString stringWithFormat:@"preset requested \"%@\"", url.lastPathComponent]);
    self.pendingPresetURL = url;
}

- (BOOL)playWavAtURL:(NSURL *)url error:(NSError **)error
{
    SVWavFeeder *wavFeeder = [[SVWavFeeder alloc] initWithURL:url error:error];
    if (wavFeeder == nil)
        return NO;

    self.wavFeeder = wavFeeder;
    self.wavURL = url;
    self.wavFrameRemainder = 0.0;
    VisualizerLog([NSString stringWithFormat:@"wav \"%@\" rate=%ld channels=%ld frames=%lu", url.lastPathComponent,
                   (long)wavFeeder.sampleRate, (long)wavFeeder.channels, (unsigned long)wavFeeder.frameCount]);
    return [self selectAudioSource:VisualizerAudioSourceWav error:error];
}

- (BOOL)selectAudioSource:(VisualizerAudioSource)audioSource error:(NSError **)error
{
    [self stopLiveInputs];
    self.audioSource = audioSource;

    SVContext *context = self.visualizerView.context;
    BOOL started = context != nil ? [self startAudioSourceWithContext:context error:error] : YES;
    if (!started)
        self.audioSource = VisualizerAudioSourceWav;

    VisualizerLog([NSString stringWithFormat:@"audio source=%@%@", VisualizerAudioSourceName(self.audioSource),
                   started ? @"" : @" (start failed, back to wav)"]);
    return started;
}

- (BOOL)startAudioSourceWithContext:(SVContext *)context error:(NSError **)error
{
    switch (self.audioSource) {
    case VisualizerAudioSourceWav:
        return YES;
    case VisualizerAudioSourceMicrophone: {
        SVAudioInput *microphoneInput = [[SVAudioInput alloc] initWithContext:context];
        if (![microphoneInput startWithError:error])
            return NO;
        self.microphoneInput = microphoneInput;
        return YES;
    }
    case VisualizerAudioSourceSystemAudio: {
        SVSystemAudioInput *systemAudioInput = [[SVSystemAudioInput alloc] initWithContext:context];
        if (systemAudioInput == nil) {
            if (error != NULL)
                *error = [NSError errorWithDomain:SVErrorDomain
                                             code:SVErrorArgument
                                         userInfo:@{ NSLocalizedDescriptionKey: @"system audio capture needs macOS 14.2 or later" }];
            return NO;
        }
        if (![systemAudioInput startWithError:error])
            return NO;
        self.systemAudioInput = systemAudioInput;
        return YES;
    }
    }
    return NO;
}

- (void)stopLiveInputs
{
    [self.microphoneInput stop];
    self.microphoneInput = nil;
    [self.systemAudioInput stop];
    self.systemAudioInput = nil;
}

- (void)applyPendingPresetWithContext:(SVContext *)context
{
    NSURL *url = self.pendingPresetURL;
    if (url == nil)
        return;

    self.pendingPresetURL = nil;
    NSError *error = nil;
    if (![context loadPresetAtURL:url blendSeconds:0.0f error:&error]) {
        VisualizerLog([NSString stringWithFormat:@"preset load failed \"%@\": %@", url.lastPathComponent, error.localizedDescription]);
        [self.delegate visualizerViewController:self didFailWithError:error];
        return;
    }

    self.presetURL = url;
    VisualizerLog([NSString stringWithFormat:@"preset loaded \"%@\"", url.lastPathComponent]);
    [self.delegate visualizerViewController:self didLoadPresetAtURL:url];
}

- (void)feedWavForDeltaTime:(NSTimeInterval)deltaTime context:(SVContext *)context
{
    SVWavFeeder *wavFeeder = self.wavFeeder;
    if (wavFeeder == nil || wavFeeder.sampleRate <= 0)
        return;

    double frames = self.wavFrameRemainder + MIN(deltaTime, VisualizerMaximumFeedSeconds) * (double)wavFeeder.sampleRate;
    double wholeFrames = floor(frames);
    self.wavFrameRemainder = frames - wholeFrames;
    if (wholeFrames < 1.0)
        return;

    wavFeeder.samplesPerRenderFrame = (NSUInteger)wholeFrames;
    [wavFeeder feedNextFrameInto:context];
}

- (void)view:(SVView *)view didCreateContext:(SVContext *)context
{
    VisualizerLog([NSString stringWithFormat:@"context created drawable=%.0fx%.0f playbackRate=%.3f slowMotion=%ld maxLuminance=%.2f flashLimit=%.2f", view.drawableSize.width, view.drawableSize.height,
        context.playbackRate, (long)context.slowMotion, context.configuration.maxLuminance, context.configuration.flashLimit]);
    [self applyPendingPresetWithContext:context];
    [self scheduleLaunchSnapshot];

    NSError *error = nil;
    if (![self startAudioSourceWithContext:context error:&error]) {
        self.audioSource = VisualizerAudioSourceWav;
        [self.delegate visualizerViewController:self didFailWithError:error];
    }
}

- (void)view:(SVView *)view willRenderFrameWithDeltaTime:(NSTimeInterval)deltaTime
{
    SVContext *context = view.context;
    if (context == nil)
        return;

    [self applyPendingPresetWithContext:context];
    if (self.audioSource == VisualizerAudioSourceWav)
        [self feedWavForDeltaTime:deltaTime context:context];
}

- (void)view:(SVView *)view didRenderFrameWithDeltaTime:(NSTimeInterval)deltaTime
{
    [self logFrameRateIfNeeded];
}

- (void)view:(SVView *)view didFailWithError:(NSError *)error
{
    VisualizerLog([NSString stringWithFormat:@"view error %ld: %@", (long)error.code, error.localizedDescription]);
    [self.delegate visualizerViewController:self didFailWithError:error];
}

@end
