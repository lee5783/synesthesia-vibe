//
//  MainWindowController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "MainWindowController.h"

#import "InspectorViewController.h"
#import "SampleBundle.h"
#import "SidebarViewController.h"
#import "VisualizerContentViewController.h"
#import "VisualizerViewController.h"

static const CGFloat MainWindowSidebarWidth = 260.0;
static const CGFloat MainWindowInspectorWidth = 320.0;
static const CGFloat MainWindowMinimumWidth = 1060.0;
static const CGFloat MainWindowMinimumHeight = 560.0;
static const NSTimeInterval MainWindowTickInterval = 0.5;
static const double MainWindowDefaultAutoChangeInterval = 30.0;
static const NSTimeInterval MainWindowDenialHintSeconds = 4.0;
static NSString *const MainWindowDefaultPresetName = @"shifter - swarm.milk";
static NSToolbarItemIdentifier const MainWindowTransportItemIdentifier = @"transport";
static NSToolbarItemIdentifier const MainWindowInspectorItemIdentifier = @"toggleInspector";

static NSString *MainWindowSourceLabel(VisualizerAudioSource source)
{
    switch (source) {
    case VisualizerAudioSourceNone:
        return @"None";
    case VisualizerAudioSourceWav:
        return @"Bundled WAV";
    case VisualizerAudioSourceMicrophone:
        return @"Microphone";
    case VisualizerAudioSourceSystemAudio:
        return @"System sound";
    }
    return @"Unknown";
}

static VisualizerAudioSource MainWindowAudioSourceForIndex(NSInteger index)
{
    switch (index) {
    case 0:
        return VisualizerAudioSourceNone;
    case 2:
        return VisualizerAudioSourceMicrophone;
    case 3:
        return VisualizerAudioSourceSystemAudio;
    default:
        return VisualizerAudioSourceWav;
    }
}

static NSInteger MainWindowIndexForAudioSource(VisualizerAudioSource source)
{
    switch (source) {
    case VisualizerAudioSourceNone:
        return 0;
    case VisualizerAudioSourceWav:
        return 1;
    case VisualizerAudioSourceMicrophone:
        return 2;
    case VisualizerAudioSourceSystemAudio:
        return 3;
    }
    return 1;
}

@interface MainWindowController () <NSWindowDelegate, NSToolbarDelegate, SidebarViewControllerDelegate, InspectorViewControllerDelegate, VisualizerViewControllerDelegate, VisualizerContentViewControllerDelegate>

@property (nonatomic, strong) NSSplitViewController *splitViewController;
@property (nonatomic, strong) SidebarViewController *sidebarViewController;
@property (nonatomic, strong) VisualizerContentViewController *visualizerContentViewController;
@property (nonatomic, strong) InspectorViewController *inspectorViewController;
@property (nonatomic, strong) VisualizerViewController *visualizerViewController;
@property (nonatomic, strong) NSSplitViewItem *sidebarItem;
@property (nonatomic, strong) NSSplitViewItem *inspectorItem;
@property (nonatomic, strong) NSSegmentedControl *transportControl;
@property (nonatomic, copy) NSArray<NSURL *> *presetURLs;
@property (nonatomic, copy, nullable) NSURL *currentPresetURL;
@property (nonatomic, assign) BOOL isAutoChangeOn;
@property (nonatomic, assign) double autoChangeInterval;
@property (nonatomic, assign) double secondsUntilChange;
@property (nonatomic, assign) BOOL isDebugOverlayOn;
@property (nonatomic, assign) double flatLevelSeconds;
@property (nonatomic, assign) BOOL didShowDenialHint;
@property (nonatomic, strong, nullable) NSTimer *tickTimer;
@property (nonatomic, assign) BOOL isCollapsingForFullScreen;

@end

@implementation MainWindowController

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0.0, 0.0, MainWindowMinimumWidth, 680.0)
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.title = @"SynesthesiaVibe AppKit";
    window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
    window.contentMinSize = NSMakeSize(MainWindowMinimumWidth, MainWindowMinimumHeight);
    window.releasedWhenClosed = NO;
    window.restorable = NO;
    window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];

    self = [super initWithWindow:window];
    if (self) {
        _presetURLs = [SampleBundle.presetURLs copy];
        _autoChangeInterval = MainWindowDefaultAutoChangeInterval;
        _secondsUntilChange = MainWindowDefaultAutoChangeInterval;
        window.delegate = self;
        [self buildContent];
        [window center];
        [self startTickTimer];
        [self playWavAtURL:SampleBundle.defaultWavURL];
        [self selectDefaultPreset];
    }
    return self;
}

- (void)buildContent
{
    self.visualizerViewController = [[VisualizerViewController alloc] init];
    self.visualizerViewController.delegate = self;

    self.visualizerContentViewController = [[VisualizerContentViewController alloc] initWithVisualizerViewController:self.visualizerViewController];
    self.visualizerContentViewController.delegate = self;

    self.sidebarViewController = [[SidebarViewController alloc] init];
    self.sidebarViewController.delegate = self;
    self.sidebarViewController.presetURLs = self.presetURLs;
    self.sidebarViewController.folderURL = SampleBundle.presetsDirectoryURL;

    self.inspectorViewController = [[InspectorViewController alloc] init];
    self.inspectorViewController.delegate = self;
    self.inspectorViewController.systemAudioAvailable = SVSystemAudioInput.supported;

    self.sidebarItem = [NSSplitViewItem sidebarWithViewController:self.sidebarViewController];
    self.sidebarItem.minimumThickness = 220.0;
    self.sidebarItem.maximumThickness = MainWindowSidebarWidth;
    self.sidebarItem.canCollapse = YES;

    NSSplitViewItem *contentItem = [NSSplitViewItem splitViewItemWithViewController:self.visualizerContentViewController];
    contentItem.minimumThickness = 480.0;

    self.inspectorItem = [NSSplitViewItem splitViewItemWithViewController:self.inspectorViewController];
    self.inspectorItem.minimumThickness = 280.0;
    self.inspectorItem.maximumThickness = MainWindowInspectorWidth;
    self.inspectorItem.canCollapse = YES;

    self.splitViewController = [[NSSplitViewController alloc] init];
    [self.splitViewController addSplitViewItem:self.sidebarItem];
    [self.splitViewController addSplitViewItem:contentItem];
    [self.splitViewController addSplitViewItem:self.inspectorItem];

    self.window.contentViewController = self.splitViewController;
    [self buildToolbar];
    [self applyDebugOverlayVisibility];
    [self updateInspectorState];
}

- (void)buildToolbar
{
    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"MainWindowToolbar"];
    toolbar.delegate = self;
    toolbar.allowsUserCustomization = NO;
    self.window.toolbar = toolbar;
}

- (void)startTickTimer
{
    __weak MainWindowController *weakSelf = self;
    self.tickTimer = [NSTimer scheduledTimerWithTimeInterval:MainWindowTickInterval repeats:YES block:^(NSTimer *timer) {
        [weakSelf tick];
    }];
}

- (void)tick
{
    [self updateAudioDenialHint];
    if (self.isDebugOverlayOn)
        [self updateDebugOverlay];
    if (!self.isAutoChangeOn || self.visualizerViewController.isPaused)
        return;

    self.secondsUntilChange -= MainWindowTickInterval;
    if (self.secondsUntilChange > 0.0) {
        [self updateInspectorState];
        return;
    }
    [self stepBy:1];
}

- (void)selectDefaultPreset
{
    NSUInteger index = [self.presetURLs indexOfObjectPassingTest:^BOOL(NSURL *url, NSUInteger idx, BOOL *stop) {
        return [url.lastPathComponent isEqualToString:MainWindowDefaultPresetName];
    }];
    if (index == NSNotFound)
        index = 0;
    if (index >= self.presetURLs.count)
        return;
    [self loadPresetURL:self.presetURLs[index]];
}

- (void)loadPresetURL:(NSURL *)url
{
    self.currentPresetURL = url;
    [self.sidebarViewController selectPresetAtURL:url];
    [self.visualizerViewController loadPresetAtURL:url];
    [self resetCountdown];
    [self updateWindowChrome];
}

- (void)resetCountdown
{
    self.secondsUntilChange = self.autoChangeInterval;
    [self updateInspectorState];
}

- (void)stepBy:(NSInteger)offset
{
    if (self.presetURLs.count == 0)
        return;

    NSUInteger current = self.currentPresetURL != nil ? [self.presetURLs indexOfObject:self.currentPresetURL] : NSNotFound;
    NSInteger base = current == NSNotFound ? 0 : (NSInteger)current;
    NSInteger count = (NSInteger)self.presetURLs.count;
    NSInteger next = ((base + offset) % count + count) % count;
    [self loadPresetURL:self.presetURLs[(NSUInteger)next]];
}

- (void)playWavAtURL:(NSURL *)url
{
    NSError *error = nil;
    if (![self.visualizerViewController playWavAtURL:url error:&error]) {
        [self showError:error];
        return;
    }
    [self syncAudioControls];
}

- (void)syncAudioControls
{
    [self.inspectorViewController setAudioSourceIndex:MainWindowIndexForAudioSource(self.visualizerViewController.audioSource)];
}

- (void)selectAudioSource:(VisualizerAudioSource)audioSource
{
    NSError *error = nil;
    if ([self.visualizerViewController selectAudioSource:audioSource error:&error])
        [self.visualizerContentViewController showMessage:nil isError:NO];
    else
        [self showError:error];
    [self syncAudioControls];
}

- (void)updateWindowChrome
{
    self.window.title = self.currentPresetURL.URLByDeletingPathExtension.lastPathComponent ?: @"SynesthesiaVibe";
    [self updateInspectorState];
}

- (void)updateInspectorState
{
    [self.inspectorViewController setAutoChangeOn:self.isAutoChangeOn
                                         interval:self.autoChangeInterval
                                        remaining:self.secondsUntilChange];
    [self.inspectorViewController setFrameRate:self.visualizerViewController.preferredFramesPerSecond];
    [self updateWindowSubtitle];
}

- (void)updateWindowSubtitle
{
    NSUInteger index = self.currentPresetURL != nil ? [self.presetURLs indexOfObject:self.currentPresetURL] : NSNotFound;
    NSString *position = [NSString stringWithFormat:@"%lu of %lu", (unsigned long)(index == NSNotFound ? 0 : index + 1), (unsigned long)self.presetURLs.count];
    if (!self.isAutoChangeOn) {
        self.window.subtitle = [NSString stringWithFormat:@"%@ · Auto-change off", position];
        return;
    }
    self.window.subtitle = [NSString stringWithFormat:@"%@ · Next preset in %.0f s", position, ceil(self.secondsUntilChange)];
}

- (NSString *)autoChangeText
{
    if (!self.isAutoChangeOn)
        return @"off";
    return [NSString stringWithFormat:@"every %.0f s · next %.0f s", self.autoChangeInterval, ceil(self.secondsUntilChange)];
}

- (void)showError:(nullable NSError *)error
{
    [self.visualizerContentViewController showMessage:error.localizedDescription ?: @"unknown error" isError:YES];
}

- (void)updateAudioDenialHint
{
    if (self.visualizerViewController.audioSource != VisualizerAudioSourceSystemAudio || self.didShowDenialHint)
        return;

    SVAudioLevels levels = self.visualizerViewController.audioLevels;
    if (fabs(levels.bass) >= 0.01 || fabs(levels.mid) >= 0.01 || fabs(levels.treble) >= 0.01) {
        self.flatLevelSeconds = 0.0;
        return;
    }

    self.flatLevelSeconds += MainWindowTickInterval;
    if (self.flatLevelSeconds < MainWindowDenialHintSeconds)
        return;

    self.didShowDenialHint = YES;
    [self.visualizerContentViewController showMessage:@"No system sound detected. If you denied the request, allow it in System Settings › Privacy & Security › Screen & System Audio Recording."
                                    isError:YES];
}

- (void)updateDebugOverlay
{
    NSDictionary<NSString *, id> *info = self.visualizerViewController.lastDebugInfo ?: @{};
    float displayFPS = [info[@"display_fps"] floatValue];
    NSString *fpsText = [NSString stringWithFormat:@"%.1f", displayFPS];
    NSString *milliseconds = displayFPS > 0.0f ? [NSString stringWithFormat:@"%.1f", 1000.0 / displayFPS] : @"0.0";

    [self.visualizerContentViewController updateDebugOverlayWithInfo:info
                                                       cap:self.visualizerViewController.preferredFramesPerSecond
                                                   fpsText:fpsText
                                                fpsHealthy:displayFPS >= 50.0f
                                          millisecondsText:milliseconds
                                                    source:MainWindowSourceLabel(self.visualizerViewController.audioSource)
                                                sampleRate:self.visualizerViewController.audioSampleRate
                                                  backend:self.visualizerViewController.glVersionString
                                               renderScale:self.visualizerViewController.renderScale
                                                      autoChangeText:[self autoChangeText]
                                                 lastError:info[@"last_error"]
                                                    levels:self.visualizerViewController.audioLevels];
}

- (void)applyDebugOverlayVisibility
{
    [self.visualizerContentViewController updateDebugOverlayVisible:self.isDebugOverlayOn on:self.isDebugOverlayOn];
    self.visualizerViewController.debugEnabled = self.isDebugOverlayOn;
    if (self.isDebugOverlayOn)
        [self updateDebugOverlay];
}

- (void)setDebugOverlayOn:(BOOL)on
{
    self.isDebugOverlayOn = on;
    [self.inspectorViewController setDebugOverlayOn:on];
    [self applyDebugOverlayVisibility];
}

- (void)toggleDebugOverlay:(id)sender
{
    [self setDebugOverlayOn:!self.isDebugOverlayOn];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if (menuItem.action == @selector(toggleDebugOverlay:)) {
        menuItem.state = self.isDebugOverlayOn ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    return YES;
}

- (void)togglePause
{
    self.visualizerViewController.paused = !self.visualizerViewController.isPaused;
    [self updateTransportIcon];
}

- (void)updateTransportIcon
{
    NSString *symbol = self.visualizerViewController.isPaused ? @"play.fill" : @"pause.fill";
    NSString *description = self.visualizerViewController.isPaused ? @"Play" : @"Pause";
    [self.transportControl setImage:[NSImage imageWithSystemSymbolName:symbol accessibilityDescription:description] forSegment:1];
}

- (void)toggleInspector:(id)sender
{
    self.inspectorItem.animator.collapsed = !self.inspectorItem.isCollapsed;
}

#pragma mark - SidebarViewControllerDelegate

- (void)sidebarViewController:(SidebarViewController *)controller didSelectPresetAtURL:(NSURL *)url
{
    if ([url isEqual:self.currentPresetURL])
        return;
    [self loadPresetURL:url];
}

- (void)sidebarViewController:(SidebarViewController *)controller didChooseFolderAtURL:(NSURL *)url
{
    NSArray<NSURL *> *presets = [SampleBundle presetURLsInDirectory:url];
    if (presets.count == 0) {
        [self.visualizerContentViewController showMessage:@"No .milk presets in that folder" isError:YES];
        return;
    }

    self.presetURLs = presets;
    controller.presetURLs = presets;
    controller.folderURL = url;
    [self loadPresetURL:presets.firstObject];
}

#pragma mark - InspectorViewControllerDelegate

- (void)inspectorViewController:(InspectorViewController *)controller didSetAutoChange:(BOOL)on
{
    self.isAutoChangeOn = on;
    [self resetCountdown];
}

- (void)inspectorViewController:(InspectorViewController *)controller didSetFrameRate:(float)frameRate
{
    self.visualizerViewController.preferredFramesPerSecond = frameRate;
}

- (void)inspectorViewController:(InspectorViewController *)controller didSelectAudioSourceIndex:(NSInteger)index
{
    [self selectAudioSource:MainWindowAudioSourceForIndex(index)];
}

- (void)inspectorViewController:(InspectorViewController *)controller didSetDebugOverlay:(BOOL)on
{
    [self setDebugOverlayOn:on];
}

#pragma mark - VisualizerContentViewControllerDelegate

- (void)visualizerContentViewControllerDidToggleDebugOverlay:(VisualizerContentViewController *)controller
{
    [self setDebugOverlayOn:!self.isDebugOverlayOn];
}

- (void)visualizerContentViewControllerDidToggleFullScreen:(VisualizerContentViewController *)controller
{
    [self.window toggleFullScreen:nil];
}

#pragma mark - VisualizerViewControllerDelegate

- (void)visualizerViewController:(VisualizerViewController *)controller didLoadPresetAtURL:(NSURL *)url
{
    self.currentPresetURL = url;
    [self updateWindowChrome];
}

- (void)visualizerViewController:(VisualizerViewController *)controller didFailWithError:(NSError *)error
{
    [self showError:error];
    [self syncAudioControls];
}

- (void)visualizerViewController:(VisualizerViewController *)controller didUpdateDebugInfo:(NSDictionary<NSString *, id> *)info
{
    if (self.isDebugOverlayOn)
        [self updateDebugOverlay];
}

#pragma mark - NSToolbarDelegate

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    return @[ NSToolbarFlexibleSpaceItemIdentifier, MainWindowTransportItemIdentifier, MainWindowInspectorItemIdentifier ];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    return @[ NSToolbarFlexibleSpaceItemIdentifier, MainWindowTransportItemIdentifier, MainWindowInspectorItemIdentifier ];
}

- (nullable NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
    if ([itemIdentifier isEqualToString:MainWindowTransportItemIdentifier])
        return [self transportToolbarItem];
    if ([itemIdentifier isEqualToString:MainWindowInspectorItemIdentifier])
        return [self inspectorToolbarItem];
    return nil;
}

- (NSToolbarItem *)transportToolbarItem
{
    self.transportControl = [NSSegmentedControl segmentedControlWithImages:@[
        [NSImage imageWithSystemSymbolName:@"backward.end" accessibilityDescription:@"Previous preset"],
        [NSImage imageWithSystemSymbolName:@"pause.fill" accessibilityDescription:@"Pause"],
        [NSImage imageWithSystemSymbolName:@"forward.end" accessibilityDescription:@"Next preset"]
    ]
                                                              trackingMode:NSSegmentSwitchTrackingMomentary
                                                                    target:self
                                                                    action:@selector(transportChanged:)];
    self.transportControl.segmentStyle = NSSegmentStyleTexturedRounded;
    self.transportControl.accessibilityLabel = @"Playback";

    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:MainWindowTransportItemIdentifier];
    item.view = self.transportControl;
    item.label = @"Playback";
    return item;
}

- (NSToolbarItem *)inspectorToolbarItem
{
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:MainWindowInspectorItemIdentifier];
    item.image = [NSImage imageWithSystemSymbolName:@"sidebar.right" accessibilityDescription:@"Hide inspector"];
    item.label = @"Hide inspector";
    item.toolTip = @"Hide inspector";
    item.target = self;
    item.action = @selector(toggleInspector:);
    item.bordered = YES;
    return item;
}

- (void)transportChanged:(NSSegmentedControl *)sender
{
    switch (sender.selectedSegment) {
    case 0:
        [self stepBy:-1];
        break;
    case 1:
        [self togglePause];
        break;
    case 2:
        [self stepBy:1];
        break;
    default:
        break;
    }
}

#pragma mark - NSWindowDelegate

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    self.isCollapsingForFullScreen = YES;
    self.sidebarItem.collapsed = YES;
    self.inspectorItem.collapsed = YES;
    self.visualizerContentViewController.fullScreen = YES;
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
    if (!self.isCollapsingForFullScreen)
        return;
    self.isCollapsingForFullScreen = NO;
    self.sidebarItem.collapsed = NO;
    self.inspectorItem.collapsed = NO;
    self.visualizerContentViewController.fullScreen = NO;
}

- (void)windowDidResize:(NSNotification *)notification
{
}

- (void)windowWillClose:(NSNotification *)notification
{
    [self.tickTimer invalidate];
    self.tickTimer = nil;
}

@end
