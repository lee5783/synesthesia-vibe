//
//  SVSyncGroup.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

#import "SVSyncGroup+Internal.h"
#import "SVContext+Internal.h"

#include <TargetConditionals.h>

#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#import <IOSurface/IOSurface.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/CGLIOSurface.h>
#import <OpenGL/gl3.h>
#else
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES3/gl.h>
#endif
#import <QuartzCore/QuartzCore.h>

#include <math.h>
#include <stdio.h>

#if TARGET_OS_OSX
@compatibility_alias SVSyncNativeContext NSOpenGLContext;
#else
@compatibility_alias SVSyncNativeContext EAGLContext;
#endif

namespace
{
    constexpr NSTimeInterval kDefaultFrameDeltaTime = 1.0 / 60.0;
    constexpr double kFrameDeadlineTolerance = 0.2;
    constexpr double kMinimumFrameRateCap = 1.0;
    constexpr double kFallbackFramesPerSecond = 60.0;
    constexpr CGFloat kMinimumRenderScale = 0.25;
    constexpr CGFloat kMaximumRenderScale = 1.0;
    constexpr NSInteger kPlaceholderWidth = 640;
    constexpr NSInteger kPlaceholderHeight = 360;

    struct SVSyncBlitRects
    {
        GLint source[4];
        GLint target[4];
    };

    float SVSyncNormalizedFramesPerSecond(float framesPerSecond)
    {
        return isfinite(framesPerSecond) && framesPerSecond > 0.0f ? fmaxf(framesPerSecond, static_cast<float>(kMinimumFrameRateCap)) : 0.0f;
    }

    CGFloat SVSyncClampedRenderScale(CGFloat renderScale)
    {
        return isfinite(renderScale) ? fmin(fmax(renderScale, kMinimumRenderScale), kMaximumRenderScale) : kMaximumRenderScale;
    }

    double SVSyncMemberFramesPerSecond(SVView *view)
    {
        const NSInteger screenMaximum = view.window.screen.maximumFramesPerSecond;
        const double maximum = screenMaximum > 0 ? static_cast<double>(screenMaximum) : kFallbackFramesPerSecond;
        const float cap = view.preferredFramesPerSecond;
        return cap > 0.0f ? fmin(static_cast<double>(cap), maximum) : maximum;
    }

    SVSyncBlitRects SVSyncBlitRectsForScaling(CGSize source, CGSize target, SVSyncScaling scaling)
    {
        SVSyncBlitRects rects = { { 0, 0, static_cast<GLint>(source.width), static_cast<GLint>(source.height) },
                                  { 0, 0, static_cast<GLint>(target.width), static_cast<GLint>(target.height) } };
        if (scaling == SVSyncScalingStretch || source.width < 1.0 || source.height < 1.0 || target.width < 1.0 || target.height < 1.0)
            return rects;

        const double sourceAspect = source.width / source.height;
        const double targetAspect = target.width / target.height;

        if (scaling == SVSyncScalingFit) {
            if (sourceAspect > targetAspect) {
                const GLint height = static_cast<GLint>(lround(target.width / sourceAspect));
                const GLint y = (static_cast<GLint>(target.height) - height) / 2;
                rects.target[1] = y;
                rects.target[3] = y + height;
            } else {
                const GLint width = static_cast<GLint>(lround(target.height * sourceAspect));
                const GLint x = (static_cast<GLint>(target.width) - width) / 2;
                rects.target[0] = x;
                rects.target[2] = x + width;
            }
            return rects;
        }

        if (sourceAspect > targetAspect) {
            const GLint width = static_cast<GLint>(lround(source.height * targetAspect));
            const GLint x = (static_cast<GLint>(source.width) - width) / 2;
            rects.source[0] = x;
            rects.source[2] = x + width;
        } else {
            const GLint height = static_cast<GLint>(lround(source.width / targetAspect));
            const GLint y = (static_cast<GLint>(source.height) - height) / 2;
            rects.source[1] = y;
            rects.source[3] = y + height;
        }
        return rects;
    }

    SVSyncNativeContext *SVSyncCurrentNativeContext()
    {
#if TARGET_OS_OSX
        return [NSOpenGLContext currentContext];
#else
        return [EAGLContext currentContext];
#endif
    }

    void SVSyncMakeNativeContextCurrent(SVSyncNativeContext *_Nullable context)
    {
#if TARGET_OS_OSX
        if (context != nil)
            [context makeCurrentContext];
        else
            [NSOpenGLContext clearCurrentContext];
#else
        [EAGLContext setCurrentContext:context];
#endif
    }
}

@interface SVSyncGroup ()

@property (nonatomic, assign, readonly) CGSize surfaceSize;
#if TARGET_OS_OSX
@property (nonatomic, assign, readonly, nullable) IOSurfaceRef surface;
#else
@property (nonatomic, assign, readonly) GLuint surfaceTexture;
@property (nonatomic, assign, readonly) NSUInteger surfaceGeneration;
#endif

@end

@implementation SVSyncGroup
{
    SVContextConfiguration *_configuration;
    SVSyncNativeContext *_glContext;
    SVContext *_context;
    NSHashTable<SVView *> *_members;
    __weak SVView *_driver;
    CGSize _renderSize;
    CGSize _grownSize;
    CGSize _fixedRenderSize;
    CGFloat _renderScale;
    float _preferredFramesPerSecond;
    NSTimeInterval _cappedFrameInterval;
    CFTimeInterval _nextFrameDeadline;
    CFTimeInterval _lastRenderTime;
    BOOL _paused;
    GLuint _surfaceTexture;
    GLuint _surfaceFramebuffer;
    CGSize _surfaceSize;
#if TARGET_OS_OSX
    IOSurfaceRef _surface;
#else
    NSUInteger _surfaceGeneration;
#endif
}

- (nullable instancetype)initWithConfiguration:(SVContextConfiguration *)configuration error:(NSError **)error
{
    self = [super init];
    if (self == nil)
        return nil;

    if (configuration.deterministic) {
        if (error != nullptr)
            *error = SVMakeError(SVErrorArgument, @"SVSyncGroup refuses a deterministic configuration");
        return nil;
    }

    _glContext = [SVSyncGroup createNativeContext];
    if (_glContext == nil) {
        if (error != nullptr)
            *error = SVMakeError(SVErrorRendererUnavailable, @"renderer unavailable: SVSyncGroup could not create its GL context");
        return nil;
    }

    _configuration = [configuration copy];
    SVContextConfiguration *contextConfiguration = [configuration copy];
    if (contextConfiguration.width <= 0 || contextConfiguration.height <= 0) {
        contextConfiguration.width = kPlaceholderWidth;
        contextConfiguration.height = kPlaceholderHeight;
    }

    SVSyncNativeContext *previousContext = SVSyncCurrentNativeContext();
    SVSyncMakeNativeContextCurrent(_glContext);
    _context = [[SVContext alloc] initWithConfiguration:contextConfiguration error:error];
    SVSyncMakeNativeContextCurrent(previousContext);

    if (_context == nil)
        return nil;

    _members = [NSHashTable weakObjectsHashTable];
    _renderSize = CGSizeMake(contextConfiguration.width, contextConfiguration.height);
    _renderScale = kMaximumRenderScale;
    _preferredFramesPerSecond = SVSyncNormalizedFramesPerSecond(configuration.maxFps);
    _cappedFrameInterval = _preferredFramesPerSecond > 0.0f ? 1.0 / _preferredFramesPerSecond : 0.0;
    [_context pause];
    _paused = YES;
    fprintf(stderr, "SVSyncGroup: created %dx%d\n", static_cast<int>(_renderSize.width), static_cast<int>(_renderSize.height));
    return self;
}

- (void)dealloc
{
    SVSyncNativeContext *previousContext = SVSyncCurrentNativeContext();
    SVSyncMakeNativeContextCurrent(_glContext);
    [self releaseSurface];
    [_context invalidate];
    SVSyncMakeNativeContextCurrent(previousContext != _glContext ? previousContext : nil);
}

- (SVContextConfiguration *)configuration
{
    return [_configuration copy];
}

- (SVContext *)context
{
    return _context;
}

- (CGSize)renderSize
{
    return _renderSize;
}

- (BOOL)isPaused
{
    return _paused;
}

- (NSUInteger)memberCount
{
    return _members.allObjects.count;
}

- (CGSize)fixedRenderSize
{
    return _fixedRenderSize;
}

- (void)setFixedRenderSize:(CGSize)fixedRenderSize
{
    _fixedRenderSize = fixedRenderSize.width >= 1.0 && fixedRenderSize.height >= 1.0
        ? CGSizeMake(floor(fixedRenderSize.width), floor(fixedRenderSize.height))
        : CGSizeZero;
    fprintf(stderr, "SVSyncGroup: fixed render size %dx%d\n", static_cast<int>(_fixedRenderSize.width), static_cast<int>(_fixedRenderSize.height));
}

- (CGFloat)renderScale
{
    return _renderScale;
}

- (void)setRenderScale:(CGFloat)renderScale
{
    _renderScale = SVSyncClampedRenderScale(renderScale);
    fprintf(stderr, "SVSyncGroup: render scale %.2f\n", _renderScale);
}

- (float)preferredFramesPerSecond
{
    return _preferredFramesPerSecond;
}

- (void)setPreferredFramesPerSecond:(float)preferredFramesPerSecond
{
    _preferredFramesPerSecond = SVSyncNormalizedFramesPerSecond(preferredFramesPerSecond);
    _cappedFrameInterval = _preferredFramesPerSecond > 0.0f ? 1.0 / _preferredFramesPerSecond : 0.0;
    _nextFrameDeadline = 0.0;
    fprintf(stderr, "SVSyncGroup: preferred frames per second %.2f\n", _preferredFramesPerSecond);
}

- (float)playbackRate
{
    return _context.playbackRate;
}

- (void)setPlaybackRate:(float)playbackRate
{
    _context.playbackRate = SVNormalizedPlaybackRate(playbackRate);
    fprintf(stderr, "SVSyncGroup: playback rate %.3f\n", _context.playbackRate);
}

- (void (^)(NSDictionary<NSString *, id> *))debugInfoHandler
{
    return _context.debugInfoHandler;
}

- (void)setDebugInfoHandler:(void (^)(NSDictionary<NSString *, id> *))debugInfoHandler
{
    _context.debugInfoHandler = debugInfoHandler;
}

- (float)debugInterval
{
    return _context.debugInterval;
}

- (void)setDebugInterval:(float)debugInterval
{
    _context.debugInterval = debugInterval;
}

- (BOOL)debugEnabled
{
    return _context.debugEnabled;
}

- (void)setDebugEnabled:(BOOL)debugEnabled
{
    _context.debugEnabled = debugEnabled;
}

- (SVTransitionConfiguration *)transitionConfiguration
{
    return _context.transitionConfiguration;
}

- (void)setTransitionConfiguration:(SVTransitionConfiguration *)transitionConfiguration
{
    _context.transitionConfiguration = transitionConfiguration ?: [[SVTransitionConfiguration alloc] init];
}

- (NSInteger)blendPattern
{
    return _context.blendPattern;
}

- (void)setBlendPattern:(NSInteger)blendPattern
{
    _context.blendPattern = blendPattern;
}

- (void (^)(SVPresetRequest *))presetRequestHandler
{
    return _context.presetRequestHandler;
}

- (void)setPresetRequestHandler:(void (^)(SVPresetRequest *))presetRequestHandler
{
    _context.presetRequestHandler = presetRequestHandler;
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

    SVSyncNativeContext *previousContext = SVSyncCurrentNativeContext();
    SVSyncMakeNativeContextCurrent(_glContext);
    block();

    if (previousContext != nil && previousContext != _glContext)
        SVSyncMakeNativeContextCurrent(previousContext);
}

- (void)addMember:(SVView *)view
{
    [_members addObject:view];
    fprintf(stderr, "SVSyncGroup: member joined view=%p count=%lu\n", (__bridge void *)view, static_cast<unsigned long>(self.memberCount));
    [self refreshMembersWithReason:@"member joined"];
}

- (void)removeMember:(SVView *)view
{
    [_members removeObject:view];
    fprintf(stderr, "SVSyncGroup: member left view=%p count=%lu\n", (__bridge void *)view, static_cast<unsigned long>(self.memberCount));
    [self refreshMembersWithReason:@"member left"];
}

- (void)refreshMembersWithReason:(NSString *)reason
{
    SVView *driver = nil;
    double driverFramesPerSecond = 0.0;
    NSUInteger renderingCount = 0;
    SVView *currentDriver = _driver;

    for (SVView *member in _members.allObjects) {
        if (!member.isSyncMemberRendering)
            continue;

        renderingCount += 1;
        const double framesPerSecond = SVSyncMemberFramesPerSecond(member);
        if (driver == nil || framesPerSecond > driverFramesPerSecond || (framesPerSecond == driverFramesPerSecond && member == currentDriver)) {
            driver = member;
            driverFramesPerSecond = framesPerSecond;
        }
    }

    if (driver != currentDriver) {
        _driver = driver;
        fprintf(stderr, "SVSyncGroup: driver %p -> %p fps=%.0f rendering=%lu reason=%s\n", (__bridge void *)currentDriver, (__bridge void *)driver,
                driverFramesPerSecond, static_cast<unsigned long>(renderingCount), reason.UTF8String);
    }

    if (renderingCount == 0 && !_paused) {
        [_context pause];
        _paused = YES;
        fprintf(stderr, "SVSyncGroup: sv_pause reason=%s\n", reason.UTF8String);
    } else if (renderingCount > 0 && _paused) {
        [_context resume];
        _paused = NO;
        _lastRenderTime = 0.0;
        _nextFrameDeadline = 0.0;
        fprintf(stderr, "SVSyncGroup: sv_resume reason=%s\n", reason.UTF8String);
    }
}

- (void)renderFrameForMember:(SVView *)view
{
    if (_driver == nil)
        [self refreshMembersWithReason:@"driver gone"];

    if (view != _driver || _paused)
        return;

    const CFTimeInterval now = CACurrentMediaTime();
    if (_cappedFrameInterval > 0.0) {
        if (now < _nextFrameDeadline - _cappedFrameInterval * kFrameDeadlineTolerance)
            return;

        _nextFrameDeadline += _cappedFrameInterval;
        if (_nextFrameDeadline < now)
            _nextFrameDeadline = now + _cappedFrameInterval;
    }

    const NSTimeInterval deltaTime = _lastRenderTime > 0.0 ? now - _lastRenderTime : kDefaultFrameDeltaTime;
    _lastRenderTime = now;

    [self performWithContextCurrent:^{
        [self applyRenderSize];
        if (self->_surfaceFramebuffer == 0)
            return;

        [self->_context renderFrameWithDeltaTime:deltaTime targetFramebuffer:self->_surfaceFramebuffer];
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glFlush();
    }];
}

- (CGSize)targetRenderSize
{
    if (_fixedRenderSize.width >= 1.0 && _fixedRenderSize.height >= 1.0)
        return _fixedRenderSize;

    for (SVView *member in _members.allObjects) {
        const CGSize pixels = member.drawableSize;
        _grownSize = CGSizeMake(fmax(_grownSize.width, pixels.width), fmax(_grownSize.height, pixels.height));
    }

    if (_grownSize.width < 1.0 || _grownSize.height < 1.0)
        return _renderSize;

    return CGSizeMake(fmax(1.0, floor(_grownSize.width * _renderScale)), fmax(1.0, floor(_grownSize.height * _renderScale)));
}

- (void)applyRenderSize
{
    const CGSize target = [self targetRenderSize];

    if (!CGSizeEqualToSize(target, _renderSize)) {
        const BOOL growth = target.width > _renderSize.width || target.height > _renderSize.height;
        [_context resizeToWidth:static_cast<NSInteger>(target.width) height:static_cast<NSInteger>(target.height)];
        fprintf(stderr, "SVSyncGroup: sv_resize %dx%d -> %dx%d %s\n", static_cast<int>(_renderSize.width), static_cast<int>(_renderSize.height),
                static_cast<int>(target.width), static_cast<int>(target.height), growth ? "growth" : "shrink");
        _renderSize = target;
    }

    if (_surfaceFramebuffer != 0 && CGSizeEqualToSize(_surfaceSize, _renderSize))
        return;

    [self releaseSurface];
    if (![self allocateSurfaceWithPixelSize:_renderSize]) {
        [self releaseSurface];
        fprintf(stderr, "SVSyncGroup: surface allocation failed %dx%d\n", static_cast<int>(_renderSize.width), static_cast<int>(_renderSize.height));
        return;
    }
    fprintf(stderr, "SVSyncGroup: surface %dx%d\n", static_cast<int>(_surfaceSize.width), static_cast<int>(_surfaceSize.height));
}

- (CGSize)surfaceSize
{
    return _surfaceSize;
}

#if TARGET_OS_OSX

+ (nullable SVSyncNativeContext *)createNativeContext
{
    NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    return pixelFormat != nil ? [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil] : nil;
}

- (nullable IOSurfaceRef)surface
{
    return _surface;
}

- (BOOL)allocateSurfaceWithPixelSize:(CGSize)pixelSize
{
    const int width = static_cast<int>(pixelSize.width);
    const int height = static_cast<int>(pixelSize.height);
    NSDictionary *properties = @{
        (__bridge NSString *)kIOSurfaceWidth: @(width),
        (__bridge NSString *)kIOSurfaceHeight: @(height),
        (__bridge NSString *)kIOSurfaceBytesPerElement: @4,
        (__bridge NSString *)kIOSurfacePixelFormat: @(static_cast<unsigned int>('BGRA'))
    };
    _surface = IOSurfaceCreate((__bridge CFDictionaryRef)properties);
    if (_surface == nullptr)
        return NO;

    glGenTextures(1, &_surfaceTexture);
    glBindTexture(GL_TEXTURE_RECTANGLE, _surfaceTexture);
    const CGLError status = CGLTexImageIOSurface2D(_glContext.CGLContextObj, GL_TEXTURE_RECTANGLE, GL_RGBA, width, height, GL_BGRA,
                                                   GL_UNSIGNED_INT_8_8_8_8_REV, _surface, 0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    if (status != kCGLNoError)
        return NO;

    glGenFramebuffers(1, &_surfaceFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _surfaceFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, _surfaceTexture, 0);
    const GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
        return NO;

    _surfaceSize = pixelSize;
    return YES;
}

- (void)releaseSurface
{
    if (_surfaceFramebuffer != 0)
        glDeleteFramebuffers(1, &_surfaceFramebuffer);
    if (_surfaceTexture != 0)
        glDeleteTextures(1, &_surfaceTexture);
    if (_surface != nullptr)
        CFRelease(_surface);

    _surfaceFramebuffer = 0;
    _surfaceTexture = 0;
    _surface = nullptr;
    _surfaceSize = CGSizeZero;
}

#else

+ (nullable SVSyncNativeContext *)createNativeContext
{
    return [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
}

- (nullable EAGLContext *)createPresentContext
{
    return [[EAGLContext alloc] initWithAPI:_glContext.API sharegroup:_glContext.sharegroup];
}

- (GLuint)surfaceTexture
{
    return _surfaceTexture;
}

- (NSUInteger)surfaceGeneration
{
    return _surfaceGeneration;
}

- (BOOL)allocateSurfaceWithPixelSize:(CGSize)pixelSize
{
    glGenTextures(1, &_surfaceTexture);
    glBindTexture(GL_TEXTURE_2D, _surfaceTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, static_cast<GLsizei>(pixelSize.width), static_cast<GLsizei>(pixelSize.height));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &_surfaceFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _surfaceFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _surfaceTexture, 0);
    const GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
        return NO;

    _surfaceSize = pixelSize;
    _surfaceGeneration += 1;
    return YES;
}

- (void)releaseSurface
{
    if (_surfaceFramebuffer != 0)
        glDeleteFramebuffers(1, &_surfaceFramebuffer);
    if (_surfaceTexture != 0)
        glDeleteTextures(1, &_surfaceTexture);

    _surfaceFramebuffer = 0;
    _surfaceTexture = 0;
    _surfaceSize = CGSizeZero;
}

#endif

@end

@implementation SVSyncPresenter
{
    SVSyncGroup *_group;
    GLuint _texture;
    GLuint _readFramebuffer;
    CGSize _boundSize;
#if TARGET_OS_OSX
    IOSurfaceRef _boundSurface;
#else
    NSUInteger _boundGeneration;
#endif
}

- (nullable instancetype)initWithGroup:(SVSyncGroup *)group error:(NSError **)error
{
    self = [super init];
    if (self == nil)
        return nil;

    _group = group;
    glGenTextures(1, &_texture);
    glGenFramebuffers(1, &_readFramebuffer);

    if (_texture == 0 || _readFramebuffer == 0) {
        [self invalidate];
        if (error != nullptr)
            *error = SVMakeError(SVErrorGL, @"SVSyncGroup: member texture or framebuffer could not be created");
        return nil;
    }

    return self;
}

- (void)dealloc
{
#if TARGET_OS_OSX
    if (_boundSurface != nullptr)
        CFRelease(_boundSurface);
#endif
}

- (BOOL)presentIntoFramebuffer:(uint32_t)framebuffer pixelSize:(CGSize)pixelSize scaling:(SVSyncScaling)scaling
{
    const BOOL bound = [self bindGroupSurface];

    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean colorMask[4];
    GLfloat clearColor[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);

    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    if (!bound || scaling == SVSyncScalingFit) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (bound) {
        const SVSyncBlitRects rects = SVSyncBlitRectsForScaling(_boundSize, pixelSize, scaling);
        const BOOL exact = rects.source[2] - rects.source[0] == rects.target[2] - rects.target[0]
            && rects.source[3] - rects.source[1] == rects.target[3] - rects.target[1];
        glBindFramebuffer(GL_READ_FRAMEBUFFER, _readFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
        glBlitFramebuffer(rects.source[0], rects.source[1], rects.source[2], rects.source[3], rects.target[0], rects.target[1], rects.target[2],
                          rects.target[3], GL_COLOR_BUFFER_BIT, exact ? GL_NEAREST : GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }

    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (scissorEnabled)
        glEnable(GL_SCISSOR_TEST);

    return bound;
}

- (void)invalidate
{
    if (_readFramebuffer != 0)
        glDeleteFramebuffers(1, &_readFramebuffer);
    if (_texture != 0)
        glDeleteTextures(1, &_texture);

    _readFramebuffer = 0;
    _texture = 0;
    _boundSize = CGSizeZero;
#if TARGET_OS_OSX
    if (_boundSurface != nullptr)
        CFRelease(_boundSurface);
    _boundSurface = nullptr;
#else
    _boundGeneration = 0;
#endif
}

#if TARGET_OS_OSX

- (BOOL)bindGroupSurface
{
    IOSurfaceRef surface = _group.surface;
    if (surface == nullptr || _texture == 0)
        return NO;

    if (surface == _boundSurface)
        return YES;

    CGLContextObj cglContext = CGLGetCurrentContext();
    const CGSize size = _group.surfaceSize;
    glBindTexture(GL_TEXTURE_RECTANGLE, _texture);
    const CGLError status = CGLTexImageIOSurface2D(cglContext, GL_TEXTURE_RECTANGLE, GL_RGBA, static_cast<GLsizei>(size.width),
                                                   static_cast<GLsizei>(size.height), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    if (status != kCGLNoError)
        return NO;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, _readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, _texture, 0);
    const GLenum framebufferStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
        return NO;

    if (_boundSurface != nullptr)
        CFRelease(_boundSurface);
    CFRetain(surface);
    _boundSurface = surface;
    _boundSize = size;
    fprintf(stderr, "SVSyncGroup: member bound surface %dx%d\n", static_cast<int>(size.width), static_cast<int>(size.height));
    return YES;
}

#else

- (BOOL)bindGroupSurface
{
    const GLuint texture = _group.surfaceTexture;
    if (texture == 0 || _readFramebuffer == 0)
        return NO;

    const NSUInteger generation = _group.surfaceGeneration;
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _readFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if (generation != _boundGeneration) {
        const GLenum framebufferStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            return NO;
        }

        _boundGeneration = generation;
        _boundSize = _group.surfaceSize;
        fprintf(stderr, "SVSyncGroup: member bound shared texture %dx%d generation=%lu\n", static_cast<int>(_boundSize.width),
                static_cast<int>(_boundSize.height), static_cast<unsigned long>(generation));
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return YES;
}

#endif

@end
