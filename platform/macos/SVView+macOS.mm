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
#import <QuartzCore/QuartzCore.h>

#include <atomic>
#include <stdio.h>
#include <string.h>
#include <string>

namespace
{
    constexpr NSTimeInterval kDefaultFrameDeltaTime = 1.0 / 60.0;
    constexpr const char* kGlErrorPrefix = "GL ";
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
}

- (void)dealloc
{
    [self stopObservingWindow];
    [self tearDownDisplayLink];

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

- (CGSize)backingPixelSize
{
    NSSize backing = [self convertSizeToBacking:self.bounds.size];
    return CGSizeMake(MAX(1.0, floor(backing.width)), MAX(1.0, floor(backing.height)));
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
    self.drawableSize = pixels;
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
    _renderPending = false;

    if (_effectivelyPaused || self.window == nil)
        return;

    SVContext *context = self.context;
    if (context == nil)
        return;

    [_glContext makeCurrentContext];

    if (_needsResize)
        [self applyResize];

    const CFTimeInterval now = CACurrentMediaTime();
    const NSTimeInterval deltaTime = _lastFrameTime > 0.0 ? now - _lastFrameTime : kDefaultFrameDeltaTime;
    _lastFrameTime = now;

    id<SVViewDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:@selector(view:willRenderFrameWithDeltaTime:)])
        [delegate view:self willRenderFrameWithDeltaTime:deltaTime];

    [context renderFrameWithDeltaTime:deltaTime targetFramebuffer:0];
    [_glContext flushBuffer];
}

- (void)applyResize
{
    [_glContext update];

    CGSize pixels = [self backingPixelSize];
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
        [self renderOnMainThread];
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
    _needsResize = YES;

    if (_cvDisplayLink != nullptr) {
        CGLContextObj cglContext = _glContext.CGLContextObj;
        CGLPixelFormatObj cglPixelFormat = _glContext.pixelFormat.CGLPixelFormatObj;
        if (cglContext != nullptr && cglPixelFormat != nullptr)
            CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_cvDisplayLink, cglContext, cglPixelFormat);
    }
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
        _occluded = (window.occlusionState & NSWindowOcclusionStateVisible) == 0 && window.isVisible;
    }

    [self updatePauseStateWithReason:window != nil ? @"attached to window" : @"removed from window"];
}

- (void)stopObservingWindow
{
    NSWindow *window = _observedWindow;
    if (window != nil)
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowDidChangeOcclusionStateNotification object:window];
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
        if (_context != nil) {
            [_context pause];
            fprintf(stderr, "SVView: sv_pause reason=%s\n", reason.UTF8String);
        }
        return;
    }

    _lastFrameTime = 0.0;
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
        }
        static_cast<CADisplayLink *>(_displayLink).paused = NO;
        fprintf(stderr, "SVView: display link started (NSView.displayLink)\n");
    } else {
        if (_cvDisplayLink == nullptr) {
            CVDisplayLinkCreateWithActiveCGDisplays(&_cvDisplayLink);
            CVDisplayLinkSetOutputCallback(_cvDisplayLink, SVViewDisplayLinkCallback, (__bridge void *)self);
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
