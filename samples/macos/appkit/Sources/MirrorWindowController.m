//
//  MirrorWindowController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

#import "MirrorWindowController.h"

static const CGFloat MirrorWindowMinimumWidth = 320.0;
static const CGFloat MirrorWindowMinimumHeight = 180.0;
static const CGFloat MirrorWindowCascadeOffset = 48.0;

@interface MirrorWindowController () <SVViewDelegate>

@property (nonatomic, strong, readwrite) SVView *visualizerView;
@property (nonatomic, copy, nullable) NSURL *presetURL;

@end

@implementation MirrorWindowController

- (instancetype)initWithConfiguration:(SVContextConfiguration *)configuration
                            presetURL:(nullable NSURL *)presetURL
                          contentSize:(NSSize)contentSize
{
    NSRect contentRect = NSMakeRect(0.0, 0.0, MAX(contentSize.width, MirrorWindowMinimumWidth), MAX(contentSize.height, MirrorWindowMinimumHeight));
    NSWindow *window = [[NSWindow alloc] initWithContentRect:contentRect
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.title = @"SynesthesiaVibe Mirror";
    window.contentMinSize = NSMakeSize(MirrorWindowMinimumWidth, MirrorWindowMinimumHeight);
    window.releasedWhenClosed = NO;
    window.restorable = NO;
    window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];

    self = [super initWithWindow:window];
    if (self) {
        _presetURL = [presetURL copy];
        SVView *visualizerView = [[SVView alloc] initWithFrame:contentRect];
        visualizerView.configuration = configuration;
        visualizerView.delegate = self;
        _visualizerView = visualizerView;
        window.contentView = visualizerView;
        [window center];
        NSPoint origin = window.frame.origin;
        [window setFrameOrigin:NSMakePoint(origin.x + MirrorWindowCascadeOffset, origin.y - MirrorWindowCascadeOffset)];
    }
    return self;
}

- (void)matchContentSize:(NSSize)contentSize
{
    [self.window setContentSize:NSMakeSize(MAX(contentSize.width, MirrorWindowMinimumWidth), MAX(contentSize.height, MirrorWindowMinimumHeight))];
}

- (void)view:(SVView *)view didCreateContext:(SVContext *)context
{
    NSURL *url = self.presetURL;
    if (url == nil)
        return;

    NSError *error = nil;
    BOOL loaded = [context loadPresetAtURL:url blendSeconds:0.0f error:&error];
    NSLog(@"MacAppKitSample: mirror preset \"%@\" %@", url.lastPathComponent, loaded ? @"loaded" : error.localizedDescription);
}

- (void)view:(SVView *)view didFailWithError:(NSError *)error
{
    NSLog(@"MacAppKitSample: mirror view error %ld: %@", (long)error.code, error.localizedDescription);
}

@end
