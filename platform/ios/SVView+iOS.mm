//
//  SVView+iOS.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVView.h"
#import "SVContext+Internal.h"

#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/ES3/gl.h>
#import <QuartzCore/QuartzCore.h>

#include <math.h>
#include <stdio.h>
#include <vector>

#include "capture.h"

namespace
{
    constexpr NSTimeInterval kDefaultFrameInterval = 1.0 / 60.0;
    constexpr NSTimeInterval kMaxFrameInterval = 0.25;
    constexpr double kFrameDeadlineTolerance = 0.2;
    constexpr double kMinimumFrameRateCap = 1.0;
    constexpr CGFloat kMinimumRenderScale = 0.25;
    constexpr CGFloat kMaximumRenderScale = 1.0;

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

@interface SVView ()

@property (nonatomic, strong, readwrite, nullable) SVContext *context;
@property (nonatomic, assign, readwrite) CGSize drawableSize;

@end

@implementation SVView
{
    SVContextConfiguration *_configuration;
    EAGLContext *_glContext;
    GLuint _framebuffer;
    GLuint _colorRenderbuffer;
    CADisplayLink *_displayLink;
    CFTimeInterval _lastFrameTimestamp;
    CFTimeInterval _nextFrameDeadline;
    NSTimeInterval _cappedFrameInterval;
    float _preferredFramesPerSecond;
    BOOL _hasPreferredFramesPerSecond;
    BOOL _frameRateHintApplied;
    CGFloat _renderScale;
    float _playbackRate;
    SVSlowMotion _slowMotion;
    NSMutableArray<void (^)(CGImageRef _Nullable)> *_snapshotCompletions;
    BOOL _applicationActive;
    BOOL _contextPaused;
    BOOL _contextFailed;
}

#pragma mark - GL context plumbing

+ (Class)layerClass
{
    return [CAEAGLLayer class];
}

- (void)configureDrawableLayer
{
    CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking: @NO,
        kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
    };
}

- (BOOL)makeGlContextCurrent
{
    if (_glContext == nil)
    {
        _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];

        if (_glContext == nil)
        {
            [self failWithCode:SVErrorRendererUnavailable message:@"renderer unavailable: EAGLContext could not create an OpenGL ES 3.0 context"];
            return NO;
        }
    }

    if ([EAGLContext currentContext] != _glContext && ![EAGLContext setCurrentContext:_glContext])
    {
        [self failWithCode:SVErrorGL message:@"EAGLContext setCurrentContext failed"];
        return NO;
    }

    return YES;
}

- (BOOL)rebuildDrawable
{
    if (_framebuffer == 0)
        glGenFramebuffers(1, &_framebuffer);
    if (_colorRenderbuffer == 0)
        glGenRenderbuffers(1, &_colorRenderbuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);

    if (![_glContext renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer])
    {
        [self failWithCode:SVErrorGL message:@"renderbufferStorage:fromDrawable: failed"];
        return NO;
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _colorRenderbuffer);

    GLint backingWidth = 0;
    GLint backingHeight = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &backingWidth);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &backingHeight);
    self.drawableSize = CGSizeMake(backingWidth, backingHeight);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        [self failWithCode:SVErrorGL message:[NSString stringWithFormat:@"drawable framebuffer incomplete (0x%04x) at %dx%d", status, backingWidth, backingHeight]];
        return NO;
    }

    return backingWidth > 0 && backingHeight > 0;
}

- (void)presentDrawable
{
    glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
    [_glContext presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)destroyDrawable
{
    if (_framebuffer != 0)
        glDeleteFramebuffers(1, &_framebuffer);
    if (_colorRenderbuffer != 0)
        glDeleteRenderbuffers(1, &_colorRenderbuffer);

    _framebuffer = 0;
    _colorRenderbuffer = 0;
}

- (void)teardownGl
{
    if (_glContext == nil)
        return;

    [EAGLContext setCurrentContext:_glContext];
    [self failPendingSnapshots];
    [self.context invalidate];
    self.context = nil;
    [self destroyDrawable];
    [EAGLContext setCurrentContext:nil];
    _glContext = nil;
}

#pragma mark - Lifecycle

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];

    if (self != nil)
        [self commonInit];

    return self;
}

- (nullable instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self != nil)
        [self commonInit];

    return self;
}

- (void)commonInit
{
    _configuration = [[SVContextConfiguration alloc] init];
    _renderScale = kMaximumRenderScale;
    _playbackRate = SVNormalizedPlaybackRate(1.0f);
    _slowMotion = SVSlowMotionInterpolate;
    _applicationActive = UIApplication.sharedApplication.applicationState != UIApplicationStateBackground;
    self.contentScaleFactor = UIScreen.mainScreen.nativeScale;
    [self configureDrawableLayer];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_displayLink invalidate];
    [self teardownGl];
}

- (SVContextConfiguration *)configuration
{
    return [_configuration copy];
}

- (void)setConfiguration:(nullable SVContextConfiguration *)configuration
{
    _configuration = configuration != nil ? [configuration copy] : [[SVContextConfiguration alloc] init];
}

- (float)preferredFramesPerSecond
{
    return _hasPreferredFramesPerSecond ? _preferredFramesPerSecond : SVNormalizedFramesPerSecond(_configuration.maxFps);
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
    self.context.playbackRate = normalized;
    fprintf(stderr, "SVView: playback rate %.3f\n", normalized);
}

- (SVSlowMotion)slowMotion
{
    return _slowMotion;
}

- (void)setSlowMotion:(SVSlowMotion)slowMotion
{
    const SVSlowMotion normalized = SVNormalizedSlowMotion(slowMotion);

    if (normalized == _slowMotion)
        return;

    _slowMotion = normalized;
    self.context.slowMotion = normalized;
    fprintf(stderr, "SVView: slow motion %s\n", normalized == SVSlowMotionFractional ? "fractional" : "interpolate");
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
    UIScreen *screen = self.window.screen ?: UIScreen.mainScreen;
    self.contentScaleFactor = screen.nativeScale * _renderScale;
    fprintf(stderr, "SVView: render scale %.2f\n", clamped);
    [self setNeedsLayout];
}

- (void)setPaused:(BOOL)paused
{
    if (_paused == paused)
        return;

    _paused = paused;
    [self updateRunningState];
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];

    if (self.window != nil)
        self.contentScaleFactor = self.window.screen.nativeScale * _renderScale;

    [self updateRunningState];
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    if (_applicationActive && self.window != nil)
        [self updateDrawable];
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
    _applicationActive = NO;
    [self updateRunningState];
}

- (void)applicationDidBecomeActive:(NSNotification *)notification
{
    _applicationActive = YES;
    [self updateRunningState];
}

#pragma mark - Running state

- (BOOL)shouldRun
{
    return !_paused && _applicationActive && self.window != nil && !_contextFailed;
}

- (void)updateRunningState
{
    if ([self shouldRun])
    {
        [self updateDrawable];
        [self resumeContext];
        [self startDisplayLink];
    }
    else
    {
        [self stopDisplayLink];
        [self pauseContext];
        [self failPendingSnapshots];
    }
}

- (NSString *)runningStateReason
{
    if (!_applicationActive)
        return @"application-inactive";
    if (self.window == nil)
        return @"no-window";
    if (_paused)
        return @"paused-property";
    if (_contextFailed)
        return @"context-failed";
    return @"running";
}

- (void)pauseContext
{
    if (self.context == nil || _contextPaused)
        return;

    [self.context pause];
    _contextPaused = YES;
    fprintf(stderr, "SVView: sv_pause reason=%s\n", [self runningStateReason].UTF8String);
}

- (void)resumeContext
{
    if (self.context == nil || !_contextPaused)
        return;

    [self.context resume];
    _contextPaused = NO;
    fprintf(stderr, "SVView: sv_resume reason=%s\n", [self runningStateReason].UTF8String);
}

- (void)startDisplayLink
{
    if (_displayLink != nil)
        return;

    _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(displayLinkDidFire:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    _lastFrameTimestamp = 0;
    _nextFrameDeadline = 0;
    [self applyFrameRateCapToDisplayLink];
}

- (void)stopDisplayLink
{
    [_displayLink invalidate];
    _displayLink = nil;
}

- (void)applyPreferredFramesPerSecond
{
    _cappedFrameInterval = _preferredFramesPerSecond > 0 ? 1.0 / _preferredFramesPerSecond : 0;
    _nextFrameDeadline = 0;
    [self applyFrameRateCapToDisplayLink];
}

- (void)applyFrameRateCapToDisplayLink
{
    if (_displayLink == nil)
        return;

    if (_cappedFrameInterval <= 0)
    {
        if (_frameRateHintApplied)
        {
            if (@available(iOS 15.0, *))
                _displayLink.preferredFrameRateRange = CAFrameRateRangeDefault;
            else
                _displayLink.preferredFramesPerSecond = 0;

            _frameRateHintApplied = NO;
            fprintf(stderr, "SVView: frame rate cap off display link preferred=default\n");
        }
        return;
    }

    UIScreen *screen = self.window.screen ?: UIScreen.mainScreen;
    const NSInteger maximumFramesPerSecond = MAX(screen.maximumFramesPerSecond, static_cast<NSInteger>(1));
    const NSInteger divisor = MAX(static_cast<NSInteger>(floor(maximumFramesPerSecond * _cappedFrameInterval)), static_cast<NSInteger>(1));
    const double preferredFramesPerSecond = static_cast<double>(maximumFramesPerSecond) / divisor;

    if (@available(iOS 15.0, *))
        _displayLink.preferredFrameRateRange = CAFrameRateRangeMake(preferredFramesPerSecond, maximumFramesPerSecond, preferredFramesPerSecond);
    else
        _displayLink.preferredFramesPerSecond = static_cast<NSInteger>(ceil(preferredFramesPerSecond));

    _frameRateHintApplied = YES;
    fprintf(stderr, "SVView: frame rate cap %.2f fps display link preferred=%.2f max=%ld\n", 1.0 / _cappedFrameInterval,
            preferredFramesPerSecond, static_cast<long>(maximumFramesPerSecond));
}

- (BOOL)shouldSkipFrameAtTimestamp:(CFTimeInterval)timestamp
{
    return _cappedFrameInterval > 0 && timestamp < _nextFrameDeadline - _cappedFrameInterval * kFrameDeadlineTolerance;
}

- (void)advanceFrameDeadlineFromTimestamp:(CFTimeInterval)timestamp
{
    if (_cappedFrameInterval <= 0)
        return;

    _nextFrameDeadline += _cappedFrameInterval;

    if (_nextFrameDeadline < timestamp)
        _nextFrameDeadline = timestamp + _cappedFrameInterval;
}

#pragma mark - Rendering

- (void)updateDrawable
{
    if (self.bounds.size.width <= 0 || self.bounds.size.height <= 0 || _contextFailed)
        return;

    if (![self makeGlContextCurrent])
        return;

    const CGSize previousSize = self.drawableSize;

    if (![self rebuildDrawable])
        return;

    if (self.context == nil)
    {
        [self createContext];
        return;
    }

    if (CGSizeEqualToSize(previousSize, self.drawableSize))
        return;

    const NSInteger width = (NSInteger)self.drawableSize.width;
    const NSInteger height = (NSInteger)self.drawableSize.height;
    NSString *errorBefore = self.context.lastErrorMessage;
    [self.context resizeToWidth:width height:height];
    fprintf(stderr, "SVView: sv_resize %ldx%ld backing pixels bounds=%.0fx%.0f scale=%.2f\n", static_cast<long>(width), static_cast<long>(height),
            self.bounds.size.width, self.bounds.size.height, self.contentScaleFactor);

    NSString *errorAfter = self.context.lastErrorMessage;

    if (errorAfter != nil && ![errorAfter isEqualToString:errorBefore ?: @""] && [errorAfter hasSuffix:@"resize failed"])
        [self reportError:[NSError errorWithDomain:SVErrorDomain code:SVErrorGL userInfo:@{ NSLocalizedDescriptionKey: errorAfter }]];

    [self reportGlErrorsWithStage:@"resize"];
}

- (void)createContext
{
    SVContextConfiguration *configuration = [_configuration copy];
    configuration.width = (NSInteger)self.drawableSize.width;
    configuration.height = (NSInteger)self.drawableSize.height;

    NSError *error = nil;
    SVContext *context = [[SVContext alloc] initWithConfiguration:configuration error:&error];

    if (context == nil)
    {
        _contextFailed = YES;
        [self stopDisplayLink];
        [self failPendingSnapshots];
        [self reportError:error];
        return;
    }

    self.context = context;
    context.playbackRate = _playbackRate;
    context.slowMotion = _slowMotion;
    _contextPaused = NO;

    if (!_hasPreferredFramesPerSecond)
    {
        _preferredFramesPerSecond = SVNormalizedFramesPerSecond(configuration.maxFps);
        _hasPreferredFramesPerSecond = YES;
    }

    [self applyPreferredFramesPerSecond];
    fprintf(stderr, "SVView: context created %ldx%ld backing pixels scale=%.2f\n", static_cast<long>(configuration.width), static_cast<long>(configuration.height), self.contentScaleFactor);

    if ([self.delegate respondsToSelector:@selector(view:didCreateContext:)])
        [self.delegate view:self didCreateContext:context];

    [self reportGlErrorsWithStage:@"create"];
}

- (void)displayLinkDidFire:(CADisplayLink *)displayLink
{
    if (![self shouldRun] || self.context == nil || _framebuffer == 0)
        return;

    const CFTimeInterval timestamp = displayLink.timestamp;

    if ([self shouldSkipFrameAtTimestamp:timestamp])
        return;

    if (![self makeGlContextCurrent])
        return;

    NSTimeInterval deltaTime = _lastFrameTimestamp > 0 ? timestamp - _lastFrameTimestamp : kDefaultFrameInterval;
    _lastFrameTimestamp = timestamp;
    [self advanceFrameDeadlineFromTimestamp:timestamp];

    const NSTimeInterval maxFrameInterval = _cappedFrameInterval > 0 ? fmax(kMaxFrameInterval, 2.0 * _cappedFrameInterval) : kMaxFrameInterval;

    if (deltaTime <= 0 || deltaTime > maxFrameInterval)
        deltaTime = kDefaultFrameInterval;

    if ([self.delegate respondsToSelector:@selector(view:willRenderFrameWithDeltaTime:)])
        [self.delegate view:self willRenderFrameWithDeltaTime:deltaTime];

    if (self.context == nil || ![self shouldRun])
        return;

    [self.context renderFrameWithDeltaTime:deltaTime targetFramebuffer:_framebuffer];
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    [self completePendingSnapshots];

    if ([self.delegate respondsToSelector:@selector(view:didRenderFrameWithDeltaTime:)])
        [self.delegate view:self didRenderFrameWithDeltaTime:deltaTime];

    [self presentDrawable];
}

- (void)performWithContextCurrent:(void (NS_NOESCAPE ^)(void))block
{
    if (block == nil)
        return;

    if (![NSThread isMainThread])
    {
        dispatch_sync(dispatch_get_main_queue(), ^{
            [self performWithContextCurrent:block];
        });
        return;
    }

    EAGLContext *previousContext = [EAGLContext currentContext];

    if (![self makeGlContextCurrent])
        return;

    block();

    if (previousContext != nil && previousContext != _glContext)
        [EAGLContext setCurrentContext:previousContext];
}

- (void)snapshotWithCompletion:(void (^)(CGImageRef _Nullable image))completion
{
    if (completion == nil)
        return;

    if (![self shouldRun])
    {
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

#pragma mark - Errors

- (void)reportGlErrorsWithStage:(NSString *)stage
{
    GLenum glError = glGetError();

    while (glError != GL_NO_ERROR)
    {
        fprintf(stderr, "SVView: GL error 0x%04x after %s\n", glError, stage.UTF8String);
        glError = glGetError();
    }
}

- (void)failWithCode:(SVErrorCode)code message:(NSString *)message
{
    [self reportError:[NSError errorWithDomain:SVErrorDomain code:code userInfo:@{ NSLocalizedDescriptionKey: message }]];
}

- (void)reportError:(nullable NSError *)error
{
    if (error == nil)
        return;

    fprintf(stderr, "SVView: error %ld %s\n", static_cast<long>(error.code), error.localizedDescription.UTF8String ?: "");

    if ([self.delegate respondsToSelector:@selector(view:didFailWithError:)])
        [self.delegate view:self didFailWithError:error];
}

@end
