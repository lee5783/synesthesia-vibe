//
//  VisualizerContentViewController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import "VisualizerContentViewController.h"

#import <OpenGL/OpenGL.h>

#import "DebugOverlayView.h"
#import "OverVideoButtonView.h"
#import "SVColorTokens.h"
#import "VisualizerViewController.h"

static const CGFloat ContentOverlayMargin = 12.0;

@interface VisualizerContentViewController ()

@property (nonatomic, strong) VisualizerViewController *visualizerViewController;
@property (nonatomic, strong) DebugOverlayView *debugOverlay;
@property (nonatomic, strong) OverVideoButtonView *debugButton;
@property (nonatomic, strong) OverVideoButtonView *fullScreenButton;
@property (nonatomic, strong) NSTextField *messageLabel;
@property (nonatomic, assign) BOOL surfaceOrderApplied;

@end

@implementation VisualizerContentViewController

- (instancetype)initWithVisualizerViewController:(VisualizerViewController *)visualizerViewController
{
    self = [super initWithNibName:nil bundle:nil];
    if (self)
        _visualizerViewController = visualizerViewController;
    return self;
}

- (void)loadView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 838.0, 441.0)];
    view.wantsLayer = YES;
    view.layer.backgroundColor = SVColorTokens.letterbox.CGColor;

    NSView *visualizerView = self.visualizerViewController.view;
    visualizerView.translatesAutoresizingMaskIntoConstraints = NO;
    [view addSubview:visualizerView];

    self.debugOverlay = [[DebugOverlayView alloc] initWithFrame:NSZeroRect];
    self.debugOverlay.translatesAutoresizingMaskIntoConstraints = NO;
    self.debugOverlay.hidden = YES;
    self.debugOverlay.accessibilityLabel = @"Debug overlay";

    self.debugButton = [[OverVideoButtonView alloc] initWithSymbolName:@"ladybug"
                                                                 label:@"Debug overlay"
                                                               toolTip:@"Debug Overlay (⇧⌘D)"];
    self.debugButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.debugButton.target = self;
    self.debugButton.action = @selector(toggleDebugOverlay:);

    self.fullScreenButton = [[OverVideoButtonView alloc] initWithSymbolName:@"arrow.up.left.and.arrow.down.right"
                                                                      label:@"Enter full screen"
                                                                    toolTip:@"Enter Full Screen (⌃⌘F)"];
    self.fullScreenButton.translatesAutoresizingMaskIntoConstraints = NO;
    self.fullScreenButton.target = self;
    self.fullScreenButton.action = @selector(toggleFullScreen:);

    self.messageLabel = [NSTextField wrappingLabelWithString:@""];
    self.messageLabel.font = [NSFont systemFontOfSize:11.0];
    self.messageLabel.textColor = SVColorTokens.textMuted;
    self.messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.messageLabel.hidden = YES;

    [view addSubview:self.debugOverlay];
    [view addSubview:self.messageLabel];
    [view addSubview:self.debugButton];
    [view addSubview:self.fullScreenButton];

    [NSLayoutConstraint activateConstraints:@[
        [visualizerView.topAnchor constraintEqualToAnchor:view.topAnchor],
        [visualizerView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
        [visualizerView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
        [visualizerView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],

        [self.debugOverlay.topAnchor constraintEqualToAnchor:view.topAnchor constant:ContentOverlayMargin],
        [self.debugOverlay.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:ContentOverlayMargin],

        [self.messageLabel.topAnchor constraintEqualToAnchor:self.debugOverlay.bottomAnchor constant:8.0],
        [self.messageLabel.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:ContentOverlayMargin],
        [self.messageLabel.widthAnchor constraintLessThanOrEqualToConstant:260.0],

        [self.debugButton.topAnchor constraintEqualToAnchor:view.topAnchor constant:10.0],
        [self.debugButton.trailingAnchor constraintEqualToAnchor:self.fullScreenButton.leadingAnchor constant:-6.0],
        [self.fullScreenButton.topAnchor constraintEqualToAnchor:view.topAnchor constant:10.0],
        [self.fullScreenButton.trailingAnchor constraintEqualToAnchor:view.trailingAnchor constant:-10.0]
    ]];

    self.view = view;
}

- (void)viewDidAppear
{
    [super viewDidAppear];
    [self applySurfaceOrderBelowWindowContent];
}

- (void)applySurfaceOrderBelowWindowContent
{
    if (self.surfaceOrderApplied || self.visualizerViewController.visualizerView.context == nil)
        return;

    [self.visualizerViewController.visualizerView performWithContextCurrent:^{
        NSOpenGLContext *context = NSOpenGLContext.currentContext;
        CGLContextObj cglContext = context.CGLContextObj;
        if (cglContext != NULL) {
            GLint order = -1;
            CGLSetParameter(cglContext, kCGLCPSurfaceOrder, &order);
        }
    }];
    self.surfaceOrderApplied = YES;
}

- (void)toggleDebugOverlay:(id)sender
{
    [self.delegate visualizerContentViewControllerDidToggleDebugOverlay:self];
}

- (void)toggleFullScreen:(id)sender
{
    [self.delegate visualizerContentViewControllerDidToggleFullScreen:self];
}

- (void)setFullScreen:(BOOL)fullScreen
{
    _fullScreen = fullScreen;
    self.fullScreenButton.symbolName = fullScreen ? @"arrow.down.right.and.arrow.up.left" : @"arrow.up.left.and.arrow.down.right";
    self.fullScreenButton.accessibilityLabel = fullScreen ? @"Exit full screen" : @"Enter full screen";
    self.fullScreenButton.toolTip = [NSString stringWithFormat:@"%@ Full Screen (⌃⌘F)", fullScreen ? @"Exit" : @"Enter"];
}

- (void)updateDebugOverlayVisible:(BOOL)visible on:(BOOL)on
{
    self.debugOverlay.hidden = !visible;
    self.debugButton.on = on;
}

- (void)updateDebugOverlayWithInfo:(nullable NSDictionary<NSString *, id> *)info
                               cap:(float)cap
                           fpsText:(NSString *)fpsText
                        fpsHealthy:(BOOL)fpsHealthy
                  millisecondsText:(NSString *)millisecondsText
                            source:(NSString *)source
                        sampleRate:(double)sampleRate
                          backend:(nullable NSString *)backend
                       renderScale:(CGFloat)renderScale
                              autoChangeText:(NSString *)autoChangeText
                              transitionText:(NSString *)transitionText
                         lastError:(nullable NSString *)lastError
                            levels:(SVAudioLevels)levels
{
    self.debugOverlay.capText = cap > 0.0f ? [NSString stringWithFormat:@"%.0f", cap] : @"none";
    self.debugOverlay.fpsText = fpsText;
    self.debugOverlay.fpsIsHealthy = fpsHealthy;
    self.debugOverlay.frameMillisecondsText = millisecondsText;
    self.debugOverlay.presetText = info[@"preset_name"] ?: @"";
    self.debugOverlay.sizeText = [NSString stringWithFormat:@"%d×%d @%.2g",
                                  [info[@"width"] intValue], [info[@"height"] intValue], renderScale];
    self.debugOverlay.backendText = backend.length > 0 ? backend : @"";
    self.debugOverlay.audioText = [NSString stringWithFormat:@"%@ · %.1f kHz", source, sampleRate / 1000.0];
    self.debugOverlay.rateText = [NSString stringWithFormat:@"1.0× · %@", transitionText];
    self.debugOverlay.autoText = autoChangeText;
    self.debugOverlay.lastErrorText = lastError;
    self.debugOverlay.bass = levels.bass;
    self.debugOverlay.mid = levels.mid;
    self.debugOverlay.treble = levels.treble;
}

- (void)showMessage:(nullable NSString *)message isError:(BOOL)isError
{
    if (message.length == 0) {
        self.messageLabel.hidden = YES;
        self.messageLabel.stringValue = @"";
        return;
    }

    self.messageLabel.stringValue = message;
    self.messageLabel.textColor = isError ? SVColorTokens.danger : SVColorTokens.textMuted;
    self.messageLabel.hidden = NO;
}

@end
