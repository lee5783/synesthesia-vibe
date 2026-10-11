//
//  VisualizerViewController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "VisualizerViewController.h"

#import "SampleBundle.h"

static const NSTimeInterval VisualizerMaximumFeedSeconds = 0.25;

static void VisualizerLog(NSString *message)
{
    fprintf(stderr, "AppKitSample: %s\n", message.UTF8String);
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

    SVView *visualizerView = [[SVView alloc] initWithFrame:NSMakeRect(0, 0, 838, 441)];
    visualizerView.configuration = configuration;
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
    VisualizerLog([NSString stringWithFormat:@"context created drawable=%.0fx%.0f", view.drawableSize.width, view.drawableSize.height]);
    [self applyPendingPresetWithContext:context];

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

- (void)view:(SVView *)view didFailWithError:(NSError *)error
{
    VisualizerLog([NSString stringWithFormat:@"view error %ld: %@", (long)error.code, error.localizedDescription]);
    [self.delegate visualizerViewController:self didFailWithError:error];
}

@end
