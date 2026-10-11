//
//  SVView+iOS.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVView.h"

#import <OpenGLES/EAGL.h>
#import <OpenGLES/EAGLDrawable.h>
#import <OpenGLES/ES3/gl.h>
#import <QuartzCore/QuartzCore.h>

#include <stdio.h>

namespace
{
    constexpr NSTimeInterval kDefaultFrameInterval = 1.0 / 60.0;
    constexpr NSTimeInterval kMaxFrameInterval = 0.25;
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
        self.contentScaleFactor = self.window.screen.nativeScale;

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
}

- (void)stopDisplayLink
{
    [_displayLink invalidate];
    _displayLink = nil;
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
        [self reportError:error];
        return;
    }

    self.context = context;
    _contextPaused = NO;
    fprintf(stderr, "SVView: context created %ldx%ld backing pixels scale=%.2f\n", static_cast<long>(configuration.width), static_cast<long>(configuration.height), self.contentScaleFactor);

    if ([self.delegate respondsToSelector:@selector(view:didCreateContext:)])
        [self.delegate view:self didCreateContext:context];

    [self reportGlErrorsWithStage:@"create"];
}

- (void)displayLinkDidFire:(CADisplayLink *)displayLink
{
    if (![self shouldRun] || self.context == nil || _framebuffer == 0)
        return;

    if (![self makeGlContextCurrent])
        return;

    NSTimeInterval deltaTime = _lastFrameTimestamp > 0 ? displayLink.timestamp - _lastFrameTimestamp : kDefaultFrameInterval;
    _lastFrameTimestamp = displayLink.timestamp;

    if (deltaTime <= 0 || deltaTime > kMaxFrameInterval)
        deltaTime = kDefaultFrameInterval;

    if ([self.delegate respondsToSelector:@selector(view:willRenderFrameWithDeltaTime:)])
        [self.delegate view:self willRenderFrameWithDeltaTime:deltaTime];

    if (self.context == nil || ![self shouldRun])
        return;

    [self.context renderFrameWithDeltaTime:deltaTime targetFramebuffer:_framebuffer];
    [self presentDrawable];
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
