//
//  SVView+macOS.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#include <TargetConditionals.h>

#if TARGET_OS_OSX

#import "SVView.h"
#import "SVContext+Internal.h"

#import <AppKit/AppKit.h>
#import <CoreVideo/CoreVideo.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#import <QuartzCore/QuartzCore.h>

#include <atomic>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "capture.h"

namespace
{
    constexpr NSTimeInterval kDefaultFrameDeltaTime = 1.0 / 60.0;
    constexpr double kFrameDeadlineTolerance = 0.2;
    constexpr double kMinimumFrameRateCap = 1.0;
    constexpr CGFloat kMinimumRenderScale = 0.25;
    constexpr CGFloat kMaximumRenderScale = 1.0;
    constexpr const char* kGlErrorPrefix = "GL ";

    float SVNormalizedFramesPerSecond(float framesPerSecond)
    {
        return isfinite(framesPerSecond) && framesPerSecond > 0.0f ? fmaxf(framesPerSecond, static_cast<float>(kMinimumFrameRateCap)) : 0.0f;
    }

    CGFloat SVClampedRenderScale(CGFloat renderScale)
    {
        return isfinite(renderScale) ? fmin(fmax(renderScale, kMinimumRenderScale), kMaximumRenderScale) : kMaximumRenderScale;
    }

    CGImageRef SVCreateImageFromRgbTopDown(int width, int height, const std::vector<unsigned char>& pixels)
    {
        CFDataRef data = CFDataCreate(kCFAllocatorDefault, pixels.data(), static_cast<CFIndex>(pixels.size()));
        CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGImageRef image = CGImageCreate(width, height, 8, 24, static_cast<size_t>(width) * 3, colorSpace,
                                         kCGBitmapByteOrderDefault | kCGImageAlphaNone, provider, nullptr, false, kCGRenderingIntentDefault);
        CGColorSpaceRelease(colorSpace);
        CGDataProviderRelease(provider);
        CFRelease(data);
        return image;
    }
}

@interface SVViewDisplayLinkTarget : NSObject

@property (nonatomic, weak) SVView *view;

- (void)displayLinkDidFire:(id)displayLink;

@end

@interface SVView ()

@property (nonatomic, strong, readwrite, nullable) SVContext *context;
@property (nonatomic, assign, readwrite) CGSize drawableSize;

- (void)renderOnMainThread;
- (void)scheduleRenderFromDisplayLink;

@end

@implementation SVViewDisplayLinkTarget

- (void)displayLinkDidFire:(id)displayLink
{
    [self.view renderOnMainThread];
}

@end

namespace
{
    CVReturn SVViewDisplayLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* userInfo);
}

@implementation SVView
{
    SVContextConfiguration *_configuration;
    NSOpenGLContext *_glContext;
    CVDisplayLinkRef _cvDisplayLink;
    id _displayLink;
    SVViewDisplayLinkTarget *_displayLinkTarget;
    __weak NSWindow *_observedWindow;
    std::atomic<bool> _renderPending;
    BOOL _contextFailed;
    BOOL _needsResize;
    BOOL _occluded;
    BOOL _effectivelyPaused;
    BOOL _displayLinkRunning;
    CFTimeInterval _lastFrameTime;
    CFTimeInterval _nextFrameDeadline;
    NSTimeInterval _cappedFrameInterval;
    float _preferredFramesPerSecond;
    BOOL _hasPreferredFramesPerSecond;
    CGFloat _renderScale;
    float _playbackRate;
    float _debugInterval;
    BOOL _debugEnabled;
    void (^_debugInfoHandler)(NSDictionary<NSString *, id> *);
    SVTransitionConfiguration *_transitionConfiguration;
    NSInteger _blendPattern;
    void (^_presetRequestHandler)(SVPresetRequest *);
    BOOL _frameRateHintApplied;
    BOOL _surfaceBackingSizeEnabled;
    NSMutableArray<void (^)(CGImageRef _Nullable)> *_snapshotCompletions;
}

@synthesize context = _context;

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self)
        [self commonInit];
    return self;
}

- (nullable instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self)
        [self commonInit];
    return self;
}

- (void)commonInit
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    self.wantsBestResolutionOpenGLSurface = YES;
#pragma clang diagnostic pop
    _renderPending = false;
    _needsResize = YES;
    _effectivelyPaused = YES;
    _renderScale = kMaximumRenderScale;
    _playbackRate = SVNormalizedPlaybackRate(1.0f);
    _debugInterval = 1.0f;
    _transitionConfiguration = [[SVTransitionConfiguration alloc] init];
    _blendPattern = SVBlendPatternRandom;

    NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    _glContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
    GLint swapInterval = 1;
    [_glContext setValues:&swapInterval forParameter:NSOpenGLContextParameterSwapInterval];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(screenParametersDidChange:)
                                                 name:NSApplicationDidChangeScreenParametersNotification
                                               object:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationDidChangeScreenParametersNotification object:nil];
    [self stopObservingWindow];
    [self tearDownDisplayLink];
    [self failPendingSnapshots];

    if (_context != nil) {
        [_glContext makeCurrentContext];
        [_context invalidate];
        _context = nil;
    }

    [NSOpenGLContext clearCurrentContext];
    [_glContext clearDrawable];
}

- (SVContextConfiguration *)configuration
{
    if (_configuration == nil)
        _configuration = [[SVContextConfiguration alloc] init];
    return _configuration;
}

- (void)setConfiguration:(nullable SVContextConfiguration *)configuration
{
    _configuration = [configuration copy];
    _contextFailed = NO;
}

- (nullable SVContext *)context
{
    if (_context == nil && !_contextFailed)
        [self createContext];
    return _context;
}

- (void)setPaused:(BOOL)paused
{
    if (_paused == paused)
        return;

    _paused = paused;
    [self updatePauseStateWithReason:@"paused property"];
}

- (float)preferredFramesPerSecond
{
    return _hasPreferredFramesPerSecond ? _preferredFramesPerSecond : SVNormalizedFramesPerSecond(self.configuration.maxFps);
}

- (void)setPreferredFramesPerSecond:(float)preferredFramesPerSecond
{
    const float normalized = SVNormalizedFramesPerSecond(preferredFramesPerSecond);

    if (_hasPreferredFramesPerSecond && normalized == _preferredFramesPerSecond)
        return;

    _preferredFramesPerSecond = normalized;
    _hasPreferredFramesPerSecond = YES;
    fprintf(stderr, "SVView: preferred frames per second %.2f\n", normalized);
    [self applyPreferredFramesPerSecond];
}

- (float)playbackRate
{
    return _playbackRate;
}

- (void)setPlaybackRate:(float)playbackRate
{
    const float normalized = SVNormalizedPlaybackRate(playbackRate);

    if (normalized == _playbackRate)
        return;

    _playbackRate = normalized;
    _context.playbackRate = normalized;
    fprintf(stderr, "SVView: playback rate %.3f\n", normalized);
}

- (void (^)(NSDictionary<NSString *, id> *))debugInfoHandler
{
    return _debugInfoHandler;
}

- (void)setDebugInfoHandler:(void (^)(NSDictionary<NSString *, id> *))debugInfoHandler
{
    _debugInfoHandler = [debugInfoHandler copy];
    _context.debugInfoHandler = _debugInfoHandler;
}

- (float)debugInterval
{
    return _debugInterval;
}

- (void)setDebugInterval:(float)debugInterval
{
    _debugInterval = debugInterval;
    _context.debugInterval = debugInterval;
}

- (SVTransitionConfiguration *)transitionConfiguration
{
    return _transitionConfiguration;
}

- (void)setTransitionConfiguration:(SVTransitionConfiguration *)transitionConfiguration
{
    _transitionConfiguration = [transitionConfiguration copy] ?: [[SVTransitionConfiguration alloc] init];
    _context.transitionConfiguration = _transitionConfiguration;
}

- (NSInteger)blendPattern
{
    return _blendPattern;
}

- (void)setBlendPattern:(NSInteger)blendPattern
{
    _blendPattern = blendPattern;
    _context.blendPattern = blendPattern;
}

- (void (^)(SVPresetRequest *))presetRequestHandler
{
    return _presetRequestHandler;
}

- (void)setPresetRequestHandler:(void (^)(SVPresetRequest *))presetRequestHandler
{
    _presetRequestHandler = [presetRequestHandler copy];
    _context.presetRequestHandler = _presetRequestHandler;
}

- (BOOL)debugEnabled
{
    return _debugEnabled;
}

- (void)setDebugEnabled:(BOOL)debugEnabled
{
    _debugEnabled = debugEnabled;
    _context.debugEnabled = debugEnabled;
}

- (CGFloat)renderScale
{
    return _renderScale;
}

- (void)setRenderScale:(CGFloat)renderScale
{
    const CGFloat clamped = SVClampedRenderScale(renderScale);

    if (clamped == _renderScale)
        return;

    _renderScale = clamped;
    _needsResize = YES;
    fprintf(stderr, "SVView: render scale %.2f\n", clamped);
}

- (CGSize)backingPixelSize
{
    NSSize backing = [self convertSizeToBacking:self.bounds.size];
    return CGSizeMake(MAX(1.0, floor(backing.width * _renderScale)), MAX(1.0, floor(backing.height * _renderScale)));
}

- (void)createContext
{
    if (_glContext == nil) {
        _contextFailed = YES;
        [self reportError:SVMakeError(SVErrorRendererUnavailable, @"renderer unavailable: NSOpenGLContext could not be created")];
        return;
    }

    [_glContext makeCurrentContext];

    CGSize pixels = [self backingPixelSize];
    SVContextConfiguration *configuration = [self.configuration copy];
    configuration.width = static_cast<NSInteger>(pixels.width);
    configuration.height = static_cast<NSInteger>(pixels.height);

    NSError *error = nil;
    SVContext *context = [[SVContext alloc] initWithConfiguration:configuration error:&error];

    if (context == nil) {
        _contextFailed = YES;
        [self reportError:error];
        return;
    }

    _context = context;
    context.playbackRate = _playbackRate;
    context.debugInterval = _debugInterval;
    context.debugInfoHandler = _debugInfoHandler;
    context.debugEnabled = _debugEnabled;
    context.transitionConfiguration = _transitionConfiguration;
    context.blendPattern = _blendPattern;
    context.presetRequestHandler = _presetRequestHandler;
    self.drawableSize = pixels;
    if (!_hasPreferredFramesPerSecond) {
        _preferredFramesPerSecond = SVNormalizedFramesPerSecond(configuration.maxFps);
        _hasPreferredFramesPerSecond = YES;
    }
    [self applyPreferredFramesPerSecond];
    fprintf(stderr, "SVView: context created %dx%d\n", static_cast<int>(pixels.width), static_cast<int>(pixels.height));

    if (_effectivelyPaused)
        [_context pause];

    id<SVViewDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:@selector(view:didCreateContext:)])
        [delegate view:self didCreateContext:_context];
}

- (void)reportError:(NSError *)error
{
    fprintf(stderr, "SVView: error %ld %s\n", static_cast<long>(error.code), error.localizedDescription.UTF8String ?: "");

    id<SVViewDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:@selector(view:didFailWithError:)])
        [delegate view:self didFailWithError:error];
}

- (BOOL)isOpaque
{
    return YES;
}

- (void)renderOnMainThread
{
    [self renderFrameApplyingFrameRateCap:YES];
}

- (void)renderFrameApplyingFrameRateCap:(BOOL)applyFrameRateCap
{
    _renderPending = false;

    if (_effectivelyPaused || self.window == nil)
        return;

    if (applyFrameRateCap && [self shouldSkipFrameAtTime:CACurrentMediaTime()])
        return;

    SVContext *context = self.context;
    if (context == nil) {
        [self failPendingSnapshots];
        return;
    }

    [_glContext makeCurrentContext];

    if (_needsResize)
        [self applyResize];

    const CFTimeInterval now = CACurrentMediaTime();
    const NSTimeInterval deltaTime = _lastFrameTime > 0.0 ? now - _lastFrameTime : kDefaultFrameDeltaTime;
    _lastFrameTime = now;

    if (applyFrameRateCap)
        [self advanceFrameDeadlineFromTime:now];
    else
        [self restartFrameDeadlineFromTime:now];

    id<SVViewDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:@selector(view:willRenderFrameWithDeltaTime:)])
        [delegate view:self willRenderFrameWithDeltaTime:deltaTime];

    [context renderFrameWithDeltaTime:deltaTime targetFramebuffer:0];
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    [self completePendingSnapshots];

    if ([delegate respondsToSelector:@selector(view:didRenderFrameWithDeltaTime:)])
        [delegate view:self didRenderFrameWithDeltaTime:deltaTime];

    [_glContext flushBuffer];
}

- (void)performWithContextCurrent:(void (NS_NOESCAPE ^)(void))block
{
    if (block == nil)
        return;

    if (![NSThread isMainThread]) {
        dispatch_sync(dispatch_get_main_queue(), ^{
            [self performWithContextCurrent:block];
        });
        return;
    }

    NSOpenGLContext *previousContext = [NSOpenGLContext currentContext];
    [_glContext makeCurrentContext];
    block();

    if (previousContext != nil && previousContext != _glContext)
        [previousContext makeCurrentContext];
}

- (void)snapshotWithCompletion:(void (^)(CGImageRef _Nullable image))completion
{
    if (completion == nil)
        return;

    if (_effectivelyPaused || self.window == nil || _contextFailed) {
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(nullptr);
        });
        return;
    }

    if (_snapshotCompletions == nil)
        _snapshotCompletions = [NSMutableArray array];
    [_snapshotCompletions addObject:[completion copy]];
}

- (void)completePendingSnapshots
{
    if (_snapshotCompletions.count == 0)
        return;

    NSArray<void (^)(CGImageRef _Nullable)> *completions = _snapshotCompletions;
    _snapshotCompletions = nil;

    const int width = static_cast<int>(self.drawableSize.width);
    const int height = static_cast<int>(self.drawableSize.height);
    std::vector<unsigned char> pixels;
    glReadBuffer(GL_BACK);
    CGImageRef image = render_gl::ReadbackRgbTopDown(width, height, pixels) ? SVCreateImageFromRgbTopDown(width, height, pixels) : nullptr;
    fprintf(stderr, "SVView: snapshot %dx%d %s\n", width, height, image != nullptr ? "ok" : "failed");

    for (void (^completion)(CGImageRef _Nullable) in completions)
        completion(image);

    CGImageRelease(image);
}

- (void)failPendingSnapshots
{
    if (_snapshotCompletions.count == 0)
        return;

    NSArray<void (^)(CGImageRef _Nullable)> *completions = _snapshotCompletions;
    _snapshotCompletions = nil;

    for (void (^completion)(CGImageRef _Nullable) in completions)
        completion(nullptr);
}

- (void)applyPreferredFramesPerSecond
{
    _cappedFrameInterval = _preferredFramesPerSecond > 0.0f ? 1.0 / _preferredFramesPerSecond : 0.0;
    _nextFrameDeadline = 0.0;
    [self applyFrameRateCapToDisplayLink];
}

- (void)applyFrameRateCapToDisplayLink
{
    if (@available(macOS 14.0, *)) {
        if (_displayLink == nil)
            return;

        if (_cappedFrameInterval <= 0.0) {
            if (_frameRateHintApplied) {
                static_cast<CADisplayLink *>(_displayLink).preferredFrameRateRange = CAFrameRateRangeDefault;
                _frameRateHintApplied = NO;
                fprintf(stderr, "SVView: frame rate cap off display link preferred=default\n");
            }
            return;
        }

        NSScreen *screen = self.window.screen ?: NSScreen.mainScreen;
        const NSInteger maximumFramesPerSecond = MAX(screen.maximumFramesPerSecond, static_cast<NSInteger>(1));
        const NSInteger divisor = MAX(static_cast<NSInteger>(floor(maximumFramesPerSecond * _cappedFrameInterval)), static_cast<NSInteger>(1));
        const double preferredFramesPerSecond = static_cast<double>(maximumFramesPerSecond) / divisor;
        static_cast<CADisplayLink *>(_displayLink).preferredFrameRateRange = CAFrameRateRangeMake(preferredFramesPerSecond, maximumFramesPerSecond, preferredFramesPerSecond);
        _frameRateHintApplied = YES;
        fprintf(stderr, "SVView: frame rate cap %.2f fps display link preferred=%.2f max=%ld\n", 1.0 / _cappedFrameInterval,
                preferredFramesPerSecond, static_cast<long>(maximumFramesPerSecond));
    } else if (_cappedFrameInterval > 0.0) {
        fprintf(stderr, "SVView: frame rate cap %.2f fps (CVDisplayLink skip only)\n", 1.0 / _cappedFrameInterval);
    }
}

- (BOOL)shouldSkipFrameAtTime:(CFTimeInterval)time
{
    return _cappedFrameInterval > 0.0 && time < _nextFrameDeadline - _cappedFrameInterval * kFrameDeadlineTolerance;
}

- (void)advanceFrameDeadlineFromTime:(CFTimeInterval)time
{
    if (_cappedFrameInterval <= 0.0)
        return;

    _nextFrameDeadline += _cappedFrameInterval;

    if (_nextFrameDeadline < time)
        _nextFrameDeadline = time + _cappedFrameInterval;
}

- (void)restartFrameDeadlineFromTime:(CFTimeInterval)time
{
    if (_cappedFrameInterval > 0.0)
        _nextFrameDeadline = time + _cappedFrameInterval;
}

- (void)applySurfaceBackingSize:(CGSize)pixels
{
    CGLContextObj cglContext = _glContext.CGLContextObj;
    if (cglContext == nullptr)
        return;

    if (_renderScale >= kMaximumRenderScale) {
        if (_surfaceBackingSizeEnabled) {
            CGLDisable(cglContext, kCGLCESurfaceBackingSize);
            _surfaceBackingSizeEnabled = NO;
        }
        return;
    }

    const GLint size[2] = { static_cast<GLint>(pixels.width), static_cast<GLint>(pixels.height) };
    CGLSetParameter(cglContext, kCGLCPSurfaceBackingSize, size);
    CGLEnable(cglContext, kCGLCESurfaceBackingSize);
    _surfaceBackingSizeEnabled = YES;
}

- (void)applyResize
{
    CGSize pixels = [self backingPixelSize];
    [self applySurfaceBackingSize:pixels];
    [_glContext update];

    if (CGSizeEqualToSize(pixels, self.drawableSize)) {
        _needsResize = NO;
        return;
    }

    sv_context *rawContext = _context.rawContext;
    if (rawContext == nullptr)
        return;

    const std::string errorBefore = sv_last_error(rawContext);
    sv_resize(rawContext, static_cast<int>(pixels.width), static_cast<int>(pixels.height));
    const std::string errorAfter = sv_last_error(rawContext);
    const bool failed = errorAfter != errorBefore && errorAfter.compare(0, strlen(kGlErrorPrefix), kGlErrorPrefix) == 0;
    fprintf(stderr, "SVView: sv_resize %dx%d live=%d %s\n", static_cast<int>(pixels.width), static_cast<int>(pixels.height),
            self.inLiveResize ? 1 : 0, failed ? errorAfter.c_str() : "ok");

    self.drawableSize = pixels;
    _needsResize = NO;

    if (failed)
        [self reportError:SVMakeError(SVErrorGL, @(errorAfter.c_str()))];
}

- (void)drawRect:(NSRect)dirtyRect
{
    if (self.inLiveResize)
        [self renderFrameApplyingFrameRateCap:NO];
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    _needsResize = YES;
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    fprintf(stderr, "SVView: backing scale %.1f\n", self.window.backingScaleFactor);
    [self retargetDisplayLinkWithReason:@"backing properties"];
}

- (void)windowDidChangeScreen:(NSNotification *)notification
{
    [self retargetDisplayLinkWithReason:@"window screen"];
}

- (void)screenParametersDidChange:(NSNotification *)notification
{
    if (self.window != nil)
        [self retargetDisplayLinkWithReason:@"screen parameters"];
}

- (void)retargetDisplayLinkWithReason:(NSString *)reason
{
    _needsResize = YES;
    [self targetCVDisplayLinkToWindowScreen];
    [self applyFrameRateCapToDisplayLink];

    NSScreen *screen = self.window.screen;
    NSNumber *screenNumber = screen.deviceDescription[@"NSScreenNumber"];
    fprintf(stderr, "SVView: display link retarget reason=%s display=%u max=%ld\n", reason.UTF8String, screenNumber.unsignedIntValue,
            static_cast<long>(screen.maximumFramesPerSecond));
}

- (void)targetCVDisplayLinkToWindowScreen
{
    if (_cvDisplayLink == nullptr)
        return;

    NSNumber *screenNumber = self.window.screen.deviceDescription[@"NSScreenNumber"];
    if (screenNumber != nil && CVDisplayLinkSetCurrentCGDisplay(_cvDisplayLink, screenNumber.unsignedIntValue) == kCVReturnSuccess)
        return;

    CGLContextObj cglContext = _glContext.CGLContextObj;
    CGLPixelFormatObj cglPixelFormat = _glContext.pixelFormat.CGLPixelFormatObj;
    if (cglContext != nullptr && cglPixelFormat != nullptr)
        CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_cvDisplayLink, cglContext, cglPixelFormat);
}

- (void)viewWillStartLiveResize
{
    [super viewWillStartLiveResize];
    fprintf(stderr, "SVView: live resize begin\n");
}

- (void)viewDidEndLiveResize
{
    [super viewDidEndLiveResize];
    fprintf(stderr, "SVView: live resize end\n");
    _needsResize = YES;
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self stopObservingWindow];

    NSWindow *window = self.window;
    if (window != nil) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        _glContext.view = self;
#pragma clang diagnostic pop
        _needsResize = YES;
        _observedWindow = window;
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(windowDidChangeOcclusionState:)
                                                     name:NSWindowDidChangeOcclusionStateNotification
                                                   object:window];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(windowDidChangeScreen:)
                                                     name:NSWindowDidChangeScreenNotification
                                                   object:window];
        _occluded = (window.occlusionState & NSWindowOcclusionStateVisible) == 0 && window.isVisible;
    }

    [self updatePauseStateWithReason:window != nil ? @"attached to window" : @"removed from window"];
}

- (void)stopObservingWindow
{
    NSWindow *window = _observedWindow;
    if (window != nil) {
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidChangeOcclusionStateNotification object:window];
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidChangeScreenNotification object:window];
    }
    _observedWindow = nil;
}

- (void)windowDidChangeOcclusionState:(NSNotification *)notification
{
    NSWindow *window = self.window;
    _occluded = window == nil || (window.occlusionState & NSWindowOcclusionStateVisible) == 0;
    [self updatePauseStateWithReason:_occluded ? @"window occluded" : @"window visible"];
}

- (void)viewDidHide
{
    [super viewDidHide];
    [self updatePauseStateWithReason:@"view hidden"];
}

- (void)viewDidUnhide
{
    [super viewDidUnhide];
    [self updatePauseStateWithReason:@"view unhidden"];
}

- (void)updatePauseStateWithReason:(NSString *)reason
{
    const BOOL shouldPause = _paused || _occluded || self.window == nil || self.isHiddenOrHasHiddenAncestor;

    if (shouldPause == _effectivelyPaused)
        return;

    _effectivelyPaused = shouldPause;

    if (shouldPause) {
        [self stopDisplayLink];
        [self failPendingSnapshots];
        if (_context != nil) {
            [_context pause];
            fprintf(stderr, "SVView: sv_pause reason=%s\n", reason.UTF8String);
        }
        return;
    }

    _lastFrameTime = 0.0;
    _nextFrameDeadline = 0.0;
    if (_context != nil) {
        [_context resume];
        fprintf(stderr, "SVView: sv_resume reason=%s\n", reason.UTF8String);
    }
    [self startDisplayLink];
}

- (void)startDisplayLink
{
    if (_displayLinkRunning)
        return;

    if (@available(macOS 14.0, *)) {
        if (_displayLink == nil) {
            _displayLinkTarget = [[SVViewDisplayLinkTarget alloc] init];
            _displayLinkTarget.view = self;
            CADisplayLink *displayLink = [self displayLinkWithTarget:_displayLinkTarget selector:@selector(displayLinkDidFire:)];
            [displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
            _displayLink = displayLink;
            [self applyFrameRateCapToDisplayLink];
        }
        static_cast<CADisplayLink *>(_displayLink).paused = NO;
        fprintf(stderr, "SVView: display link started (NSView.displayLink)\n");
    } else {
        if (_cvDisplayLink == nullptr) {
            CVDisplayLinkCreateWithActiveCGDisplays(&_cvDisplayLink);
            CVDisplayLinkSetOutputCallback(_cvDisplayLink, SVViewDisplayLinkCallback, (__bridge void *)self);
            [self targetCVDisplayLinkToWindowScreen];
        }
        CVDisplayLinkStart(_cvDisplayLink);
        fprintf(stderr, "SVView: display link started (CVDisplayLink)\n");
    }

    _displayLinkRunning = YES;
}

- (void)stopDisplayLink
{
    if (!_displayLinkRunning)
        return;

    if (@available(macOS 14.0, *))
        static_cast<CADisplayLink *>(_displayLink).paused = YES;
    else if (_cvDisplayLink != nullptr)
        CVDisplayLinkStop(_cvDisplayLink);

    _displayLinkRunning = NO;
    fprintf(stderr, "SVView: display link stopped\n");
}

- (void)tearDownDisplayLink
{
    [self stopDisplayLink];

    if (@available(macOS 14.0, *))
        [static_cast<CADisplayLink *>(_displayLink) invalidate];
    _displayLink = nil;
    _displayLinkTarget = nil;

    if (_cvDisplayLink != nullptr) {
        CVDisplayLinkStop(_cvDisplayLink);
        CVDisplayLinkRelease(_cvDisplayLink);
        _cvDisplayLink = nullptr;
    }
}

- (void)scheduleRenderFromDisplayLink
{
    if (_renderPending.exchange(true))
        return;

    __weak SVView *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        [weakSelf renderOnMainThread];
    });
}

@end

namespace
{
    CVReturn SVViewDisplayLinkCallback(CVDisplayLinkRef, const CVTimeStamp*, const CVTimeStamp*, CVOptionFlags, CVOptionFlags*, void* userInfo)
    {
        @autoreleasepool {
            [(__bridge SVView *)userInfo scheduleRenderFromDisplayLink];
        }
        return kCVReturnSuccess;
    }
}

#endif
