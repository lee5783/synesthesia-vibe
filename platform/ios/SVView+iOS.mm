//
//  SVView+iOS.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVView.h"
#import "SVContext+Internal.h"
#import "SVSyncGroup+Internal.h"

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
    EAGLContext *_presentContext;
    GLuint _framebuffer;
    GLuint _colorRenderbuffer;
    CADisplayLink *_displayLink;
    __weak UIScreen *_displayLinkScreen;
    CFTimeInterval _lastFrameTimestamp;
    CFTimeInterval _nextFrameDeadline;
    NSTimeInterval _cappedFrameInterval;
    float _preferredFramesPerSecond;
    BOOL _hasPreferredFramesPerSecond;
    BOOL _frameRateHintApplied;
    CGFloat _renderScale;
    float _playbackRate;
    float _debugInterval;
    BOOL _debugEnabled;
    void (^_debugInfoHandler)(NSDictionary<NSString *, id> *);
    SVTransitionConfiguration *_transitionConfiguration;
    NSInteger _blendPattern;
    void (^_presetRequestHandler)(SVPresetRequest *);
    NSMutableArray<void (^)(CGImageRef _Nullable)> *_snapshotCompletions;
    BOOL _applicationActive;
    BOOL _contextPaused;
    BOOL _contextFailed;
    SVSyncGroup *_syncGroup;
    SVSyncScaling _syncScaling;
    SVSyncPresenter *_syncPresenter;
    CGSize _contextDrawableSize;
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

    if (![[self drawableContext] renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer])
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
    [[self drawableContext] presentRenderbuffer:GL_RENDERBUFFER];
}

- (EAGLContext *)drawableContext
{
    return _presentContext ?: _glContext;
}

- (BOOL)makeDrawableContextCurrent
{
    if (_presentContext == nil)
        return [self makeGlContextCurrent];

    if ([EAGLContext currentContext] != _presentContext && ![EAGLContext setCurrentContext:_presentContext])
    {
        [self failWithCode:SVErrorGL message:@"EAGLContext setCurrentContext failed for the sync present context"];
        return NO;
    }

    return YES;
}

- (void)detachDrawable
{
    if (_colorRenderbuffer != 0)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, _colorRenderbuffer);
        [[self drawableContext] renderbufferStorage:GL_RENDERBUFFER fromDrawable:nil];
    }

    [self destroyDrawable];
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
    [_context invalidate];
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
    _debugInterval = 1.0f;
    _transitionConfiguration = [[SVTransitionConfiguration alloc] init];
    _blendPattern = SVBlendPatternRandom;
    _applicationActive = UIApplication.sharedApplication.applicationState != UIApplicationStateBackground;
    self.contentScaleFactor = UIScreen.mainScreen.nativeScale;
    [self configureDrawableLayer];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(applicationWillResignActive:) name:UIApplicationWillResignActiveNotification object:nil];
    [center addObserver:self selector:@selector(applicationDidBecomeActive:) name:UIApplicationDidBecomeActiveNotification object:nil];
    [center addObserver:self selector:@selector(screenDidDisconnect:) name:UIScreenDidDisconnectNotification object:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [_displayLink invalidate];

    if (_syncGroup != nil)
    {
        [EAGLContext setCurrentContext:_presentContext];
        [_syncPresenter invalidate];
        [self detachDrawable];
        _presentContext = nil;
        SVSyncGroup *group = _syncGroup;
        dispatch_async(dispatch_get_main_queue(), ^{
            [group refreshMembersWithReason:@"member deallocated"];
        });
    }

    [self teardownGl];
}

- (nullable SVContext *)context
{
    if (_syncGroup != nil)
        return _syncGroup.context;

    return _context;
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
    [_syncGroup refreshMembersWithReason:@"member frame rate"];
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
    fprintf(stderr, "SVView: render scale %.2f\n", clamped);
    [self applyContentScaleFactor];
}

- (CGFloat)effectiveRenderScale
{
    return _syncGroup != nil ? kMaximumRenderScale : _renderScale;
}

- (void)applyContentScaleFactor
{
    UIScreen *screen = self.window.screen ?: UIScreen.mainScreen;
    self.contentScaleFactor = screen.nativeScale * [self effectiveRenderScale];
    [self setNeedsLayout];
}

- (SVSyncGroup *)syncGroup
{
    return _syncGroup;
}

- (void)setSyncGroup:(SVSyncGroup *)syncGroup
{
    if (syncGroup == _syncGroup)
        return;

    if (_syncGroup != nil)
        [self leaveSyncGroup];

    if (syncGroup != nil)
        [self joinSyncGroup:syncGroup];
}

- (SVSyncScaling)syncScaling
{
    return _syncScaling;
}

- (void)setSyncScaling:(SVSyncScaling)syncScaling
{
    _syncScaling = syncScaling;
    fprintf(stderr, "SVView: sync scaling %ld\n", static_cast<long>(syncScaling));
}

- (BOOL)isSyncMemberRendering
{
    return [self shouldRun];
}

- (void)joinSyncGroup:(SVSyncGroup *)group
{
    if (![self makeGlContextCurrent])
        return;

    EAGLContext *presentContext = [group createPresentContext];

    if (presentContext == nil || ![EAGLContext setCurrentContext:presentContext])
    {
        [EAGLContext setCurrentContext:_glContext];
        [self failWithCode:SVErrorRendererUnavailable message:@"renderer unavailable: SVSyncGroup present context could not be created"];
        return;
    }

    NSError *error = nil;
    SVSyncPresenter *presenter = [[SVSyncPresenter alloc] initWithGroup:group error:&error];
    [EAGLContext setCurrentContext:_glContext];

    if (presenter == nil)
    {
        [self reportError:error];
        return;
    }

    _contextDrawableSize = self.drawableSize;
    [self detachDrawable];
    _presentContext = presentContext;
    _syncPresenter = presenter;
    _syncGroup = group;

    if (_context != nil && !_contextPaused)
    {
        [_context pause];
        _contextPaused = YES;
        fprintf(stderr, "SVView: sv_pause reason=sync group joined\n");
    }

    fprintf(stderr, "SVView: sync group joined\n");
    [self applyContentScaleFactor];

    if ([self shouldRun])
        [self updateDrawable];

    [group addMember:self];
}

- (void)leaveSyncGroup
{
    SVSyncGroup *group = _syncGroup;

    [EAGLContext setCurrentContext:_presentContext];
    [_syncPresenter invalidate];
    [self detachDrawable];
    [EAGLContext setCurrentContext:_glContext];
    _presentContext = nil;
    _syncPresenter = nil;
    _syncGroup = nil;
    self.drawableSize = _contextDrawableSize;
    _lastFrameTimestamp = 0;
    _nextFrameDeadline = 0;
    fprintf(stderr, "SVView: sync group left\n");
    [group removeMember:self];
    [self applyContentScaleFactor];

    if ([self shouldRun])
    {
        [self updateDrawable];
        [self resumeContext];
    }
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
        self.contentScaleFactor = self.window.screen.nativeScale * [self effectiveRenderScale];

    [self retargetDisplayLinkWithReason:@"window changed"];
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

- (void)screenDidDisconnect:(NSNotification *)notification
{
    [self retargetDisplayLinkWithReason:@"screen disconnected"];
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
        if (_syncGroup == nil)
            [self resumeContext];
        [self startDisplayLink];
    }
    else
    {
        [self stopDisplayLink];
        [self pauseContext];
        [self failPendingSnapshots];
    }

    [_syncGroup refreshMembersWithReason:[self runningStateReason]];
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
    if (_context == nil || _contextPaused)
        return;

    [_context pause];
    _contextPaused = YES;
    fprintf(stderr, "SVView: sv_pause reason=%s\n", [self runningStateReason].UTF8String);
}

- (void)resumeContext
{
    if (_context == nil || !_contextPaused)
        return;

    [_context resume];
    _contextPaused = NO;
    fprintf(stderr, "SVView: sv_resume reason=%s\n", [self runningStateReason].UTF8String);
}

- (void)startDisplayLink
{
    if (_displayLink != nil)
        return;

    UIScreen *screen = self.window.screen ?: UIScreen.mainScreen;
    _displayLink = [screen displayLinkWithTarget:self selector:@selector(displayLinkDidFire:)];
    _displayLinkScreen = screen;
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    _lastFrameTimestamp = 0;
    _nextFrameDeadline = 0;
    fprintf(stderr, "SVView: display link screen=%s max=%ld\n", screen == UIScreen.mainScreen ? "main" : "external",
            static_cast<long>(screen.maximumFramesPerSecond));
    [self applyFrameRateCapToDisplayLink];
}

- (void)stopDisplayLink
{
    [_displayLink invalidate];
    _displayLink = nil;
    _displayLinkScreen = nil;
}

- (void)retargetDisplayLinkWithReason:(NSString *)reason
{
    UIScreen *screen = self.window.screen;

    if (_displayLink == nil || screen == nil || screen == _displayLinkScreen)
        return;

    fprintf(stderr, "SVView: display link retarget reason=%s screen=%s max=%ld\n", reason.UTF8String, screen == UIScreen.mainScreen ? "main" : "external",
            static_cast<long>(screen.maximumFramesPerSecond));
    [self stopDisplayLink];
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

    if (![self makeDrawableContextCurrent])
        return;

    const CGSize previousSize = self.drawableSize;

    if (![self rebuildDrawable])
        return;

    if (_syncGroup != nil)
        return;

    if (_context == nil)
    {
        [self createContext];
        return;
    }

    if (CGSizeEqualToSize(previousSize, self.drawableSize))
        return;

    const NSInteger width = (NSInteger)self.drawableSize.width;
    const NSInteger height = (NSInteger)self.drawableSize.height;
    NSString *errorBefore = _context.lastErrorMessage;
    [_context resizeToWidth:width height:height];
    fprintf(stderr, "SVView: sv_resize %ldx%ld backing pixels bounds=%.0fx%.0f scale=%.2f\n", static_cast<long>(width), static_cast<long>(height),
            self.bounds.size.width, self.bounds.size.height, self.contentScaleFactor);

    NSString *errorAfter = _context.lastErrorMessage;

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
    context.debugInterval = _debugInterval;
    context.debugInfoHandler = _debugInfoHandler;
    context.debugEnabled = _debugEnabled;
    context.transitionConfiguration = _transitionConfiguration;
    context.blendPattern = _blendPattern;
    context.presetRequestHandler = _presetRequestHandler;
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

    if (_syncGroup != nil)
    {
        [self renderSyncedFrameWithDeltaTime:deltaTime];
        return;
    }

    if ([self.delegate respondsToSelector:@selector(view:willRenderFrameWithDeltaTime:)])
        [self.delegate view:self willRenderFrameWithDeltaTime:deltaTime];

    if (_context == nil || ![self shouldRun])
        return;

    [_context renderFrameWithDeltaTime:deltaTime targetFramebuffer:_framebuffer];
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    [self completePendingSnapshots];

    if ([self.delegate respondsToSelector:@selector(view:didRenderFrameWithDeltaTime:)])
        [self.delegate view:self didRenderFrameWithDeltaTime:deltaTime];

    [self presentDrawable];
}

- (void)renderSyncedFrameWithDeltaTime:(NSTimeInterval)deltaTime
{
    SVSyncGroup *group = _syncGroup;
    id<SVViewDelegate> delegate = self.delegate;

    if ([delegate respondsToSelector:@selector(view:willRenderFrameWithDeltaTime:)])
    {
        [group performWithContextCurrent:^{
            [delegate view:self willRenderFrameWithDeltaTime:deltaTime];
        }];
    }

    if (_syncGroup != group || ![self shouldRun])
        return;

    [group renderFrameForMember:self];

    if (![self makeDrawableContextCurrent])
        return;

    [_syncPresenter presentIntoFramebuffer:_framebuffer pixelSize:self.drawableSize scaling:_syncScaling];
    [self completePendingSnapshots];

    if ([delegate respondsToSelector:@selector(view:didRenderFrameWithDeltaTime:)])
        [delegate view:self didRenderFrameWithDeltaTime:deltaTime];

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

    if (_syncGroup != nil)
    {
        [_syncGroup performWithContextCurrent:block];
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
