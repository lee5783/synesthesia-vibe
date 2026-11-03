//
//  MainWindowController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "MainWindowController.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "SampleBundle.h"
#import "VisualizerViewController.h"

static const CGFloat MainWindowSidebarWidth = 260.0;
static const NSTimeInterval MainWindowLevelsInterval = 0.25;
static NSString *const MainWindowDefaultPresetName = @"shifter - swarm.milk";
static NSUserInterfaceItemIdentifier const MainWindowPresetColumn = @"preset";
static NSArray<NSNumber *> *MainWindowFrameRates(void)
{
    return @[ @0, @60, @30, @15 ];
}
static NSArray<NSNumber *> *MainWindowRenderScales(void)
{
    return @[ @1.0, @0.75, @0.5, @0.25 ];
}

@interface MainWindowController () <NSWindowDelegate, NSTableViewDataSource, NSTableViewDelegate, VisualizerViewControllerDelegate>

@property (nonatomic, strong) VisualizerViewController *visualizerViewController;
@property (nonatomic, copy) NSArray<NSURL *> *presetURLs;
@property (nonatomic, strong) NSTableView *presetTableView;
@property (nonatomic, strong) NSTextField *wavLabel;
@property (nonatomic, strong) NSButton *microphoneCheckbox;
@property (nonatomic, strong) NSButton *systemAudioCheckbox;
@property (nonatomic, strong) NSButton *pausedCheckbox;
@property (nonatomic, strong) NSPopUpButton *frameRatePopUp;
@property (nonatomic, strong) NSPopUpButton *renderScalePopUp;
@property (nonatomic, strong) NSTextField *levelsLabel;
@property (nonatomic, strong) NSTextField *statusLabel;
@property (nonatomic, strong, nullable) NSTimer *levelsTimer;

@end

@implementation MainWindowController

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, MainWindowSidebarWidth + 838, 520)
                                                   styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                             NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.title = @"SynesthesiaVibe AppKit";
    window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
    window.contentMinSize = NSMakeSize(MainWindowSidebarWidth + 320, 360);
    window.releasedWhenClosed = NO;
    window.restorable = NO;

    self = [super initWithWindow:window];
    if (self) {
        _presetURLs = [SampleBundle.presetURLs copy];
        window.delegate = self;
        [self buildContent];
        [window center];
        [self startLevelsTimer];
        [self playWavAtURL:SampleBundle.defaultWavURL];
        [self selectDefaultPreset];
    }
    return self;
}

- (void)buildContent
{
    self.visualizerViewController = [[VisualizerViewController alloc] init];
    self.visualizerViewController.delegate = self;

    NSView *visualizerView = self.visualizerViewController.view;
    visualizerView.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *sidebar = [self buildSidebar];
    sidebar.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *contentView = self.window.contentView;
    [contentView addSubview:sidebar];
    [contentView addSubview:visualizerView];

    [NSLayoutConstraint activateConstraints:@[
        [sidebar.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
        [sidebar.topAnchor constraintEqualToAnchor:contentView.topAnchor],
        [sidebar.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor],
        [sidebar.widthAnchor constraintEqualToConstant:MainWindowSidebarWidth],
        [visualizerView.leadingAnchor constraintEqualToAnchor:sidebar.trailingAnchor],
        [visualizerView.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor],
        [visualizerView.topAnchor constraintEqualToAnchor:contentView.topAnchor],
        [visualizerView.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor]
    ]];
}

- (NSView *)buildSidebar
{
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:MainWindowPresetColumn];
    column.title = @"Presets";
    column.resizingMask = NSTableColumnAutoresizingMask;

    self.presetTableView = [[NSTableView alloc] initWithFrame:NSZeroRect];
    [self.presetTableView addTableColumn:column];
    self.presetTableView.columnAutoresizingStyle = NSTableViewUniformColumnAutoresizingStyle;
    self.presetTableView.allowsEmptySelection = NO;
    self.presetTableView.dataSource = self;
    self.presetTableView.delegate = self;

    NSScrollView *presetScrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    presetScrollView.documentView = self.presetTableView;
    presetScrollView.hasVerticalScroller = YES;
    presetScrollView.borderType = NSBezelBorder;
    [presetScrollView setContentHuggingPriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationVertical];

    self.wavLabel = [NSTextField wrappingLabelWithString:@""];
    self.wavLabel.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];

    NSButton *chooseWavButton = [NSButton buttonWithTitle:@"Choose WAV…" target:self action:@selector(chooseWav:)];
    NSButton *bundledWavButton = [NSButton buttonWithTitle:@"Bundled WAV" target:self action:@selector(useBundledWav:)];
    NSStackView *wavButtons = [NSStackView stackViewWithViews:@[ chooseWavButton, bundledWavButton ]];

    self.microphoneCheckbox = [NSButton checkboxWithTitle:@"Microphone" target:self action:@selector(toggleMicrophone:)];
    self.systemAudioCheckbox = [NSButton checkboxWithTitle:@"System audio (macOS 14.2+)" target:self action:@selector(toggleSystemAudio:)];
    self.systemAudioCheckbox.enabled = SVSystemAudioInput.supported;
    self.pausedCheckbox = [NSButton checkboxWithTitle:@"Paused" target:self action:@selector(togglePaused:)];

    self.frameRatePopUp = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    for (NSNumber *frameRate in MainWindowFrameRates())
        [self.frameRatePopUp addItemWithTitle:frameRate.floatValue > 0.0f ? [NSString stringWithFormat:@"%.0f fps", frameRate.floatValue] : @"Display rate"];
    self.frameRatePopUp.target = self;
    self.frameRatePopUp.action = @selector(changeFrameRate:);
    NSUInteger frameRateIndex = [MainWindowFrameRates() indexOfObject:@(self.visualizerViewController.preferredFramesPerSecond)];
    if (frameRateIndex == NSNotFound)
        [self.frameRatePopUp addItemWithTitle:[NSString stringWithFormat:@"%.0f fps", self.visualizerViewController.preferredFramesPerSecond]];
    [self.frameRatePopUp selectItemAtIndex:frameRateIndex != NSNotFound ? (NSInteger)frameRateIndex : self.frameRatePopUp.numberOfItems - 1];

    self.renderScalePopUp = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    for (NSNumber *renderScale in MainWindowRenderScales())
        [self.renderScalePopUp addItemWithTitle:[NSString stringWithFormat:@"Render scale %.0f%%", renderScale.doubleValue * 100.0]];
    self.renderScalePopUp.target = self;
    self.renderScalePopUp.action = @selector(changeRenderScale:);
    NSUInteger renderScaleIndex = [MainWindowRenderScales() indexOfObject:@(self.visualizerViewController.renderScale)];
    [self.renderScalePopUp selectItemAtIndex:renderScaleIndex != NSNotFound ? (NSInteger)renderScaleIndex : 0];

    NSButton *snapshotButton = [NSButton buttonWithTitle:@"Save Snapshot…" target:self action:@selector(saveSnapshot:)];

    self.levelsLabel = [NSTextField labelWithString:@""];
    self.levelsLabel.font = [NSFont monospacedDigitSystemFontOfSize:NSFont.smallSystemFontSize weight:NSFontWeightRegular];

    self.statusLabel = [NSTextField wrappingLabelWithString:@""];
    self.statusLabel.font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];

    NSStackView *sidebar = [NSStackView stackViewWithViews:@[
        presetScrollView,
        [NSTextField labelWithString:@"Audio"],
        self.wavLabel,
        wavButtons,
        self.microphoneCheckbox,
        self.systemAudioCheckbox,
        self.pausedCheckbox,
        [NSTextField labelWithString:@"View"],
        self.frameRatePopUp,
        self.renderScalePopUp,
        snapshotButton,
        self.levelsLabel,
        self.statusLabel
    ]];
    sidebar.orientation = NSUserInterfaceLayoutOrientationVertical;
    sidebar.alignment = NSLayoutAttributeLeading;
    sidebar.spacing = 8.0;
    sidebar.edgeInsets = NSEdgeInsetsMake(12.0, 12.0, 12.0, 12.0);
    [presetScrollView.widthAnchor constraintEqualToAnchor:sidebar.widthAnchor constant:-24.0].active = YES;
    [self.wavLabel.widthAnchor constraintEqualToAnchor:presetScrollView.widthAnchor].active = YES;
    [self.statusLabel.widthAnchor constraintEqualToAnchor:presetScrollView.widthAnchor].active = YES;
    return sidebar;
}

- (void)selectDefaultPreset
{
    NSUInteger index = [self.presetURLs indexOfObjectPassingTest:^BOOL(NSURL *url, NSUInteger idx, BOOL *stop) {
        return [url.lastPathComponent isEqualToString:MainWindowDefaultPresetName];
    }];
    if (index == NSNotFound)
        index = 0;
    if (index < self.presetURLs.count)
        [self.presetTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
}

- (void)startLevelsTimer
{
    __weak MainWindowController *weakSelf = self;
    self.levelsTimer = [NSTimer scheduledTimerWithTimeInterval:MainWindowLevelsInterval repeats:YES block:^(NSTimer *timer) {
        [weakSelf updateLevels];
    }];
}

- (void)updateLevels
{
    SVAudioLevels levels = self.visualizerViewController.audioLevels;
    self.levelsLabel.stringValue = [NSString stringWithFormat:@"bass %.2f  mid %.2f  treb %.2f", levels.bass, levels.mid, levels.treble];
}

- (void)showStatus:(NSString *)status isError:(BOOL)isError
{
    self.statusLabel.stringValue = status;
    self.statusLabel.textColor = isError ? NSColor.systemRedColor : NSColor.secondaryLabelColor;
}

- (void)showError:(nullable NSError *)error
{
    [self showStatus:error.localizedDescription ?: @"unknown error" isError:YES];
}

- (void)playWavAtURL:(NSURL *)url
{
    NSError *error = nil;
    if (![self.visualizerViewController playWavAtURL:url error:&error]) {
        [self showError:error];
        return;
    }
    self.wavLabel.stringValue = [NSString stringWithFormat:@"WAV: %@", url.lastPathComponent];
    [self syncAudioControls];
}

- (void)syncAudioControls
{
    VisualizerAudioSource audioSource = self.visualizerViewController.audioSource;
    self.microphoneCheckbox.state = audioSource == VisualizerAudioSourceMicrophone ? NSControlStateValueOn : NSControlStateValueOff;
    self.systemAudioCheckbox.state = audioSource == VisualizerAudioSourceSystemAudio ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)selectAudioSource:(VisualizerAudioSource)audioSource
{
    NSError *error = nil;
    if (![self.visualizerViewController selectAudioSource:audioSource error:&error])
        [self showError:error];
    [self syncAudioControls];
}

- (void)chooseWav:(id)sender
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.allowedContentTypes = @[ UTTypeWAV ];
    panel.allowsMultipleSelection = NO;
    __weak MainWindowController *weakSelf = self;
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result == NSModalResponseOK && panel.URL != nil)
            [weakSelf playWavAtURL:panel.URL];
    }];
}

- (void)useBundledWav:(id)sender
{
    [self playWavAtURL:SampleBundle.defaultWavURL];
}

- (void)toggleMicrophone:(NSButton *)sender
{
    [self selectAudioSource:sender.state == NSControlStateValueOn ? VisualizerAudioSourceMicrophone : VisualizerAudioSourceWav];
}

- (void)toggleSystemAudio:(NSButton *)sender
{
    [self selectAudioSource:sender.state == NSControlStateValueOn ? VisualizerAudioSourceSystemAudio : VisualizerAudioSourceWav];
}

- (void)togglePaused:(NSButton *)sender
{
    self.visualizerViewController.paused = sender.state == NSControlStateValueOn;
}

- (void)changeFrameRate:(NSPopUpButton *)sender
{
    NSInteger index = sender.indexOfSelectedItem;
    if (index >= 0 && (NSUInteger)index < MainWindowFrameRates().count)
        self.visualizerViewController.preferredFramesPerSecond = MainWindowFrameRates()[(NSUInteger)index].floatValue;
}

- (void)changeRenderScale:(NSPopUpButton *)sender
{
    NSInteger index = sender.indexOfSelectedItem;
    if (index >= 0 && (NSUInteger)index < MainWindowRenderScales().count)
        self.visualizerViewController.renderScale = (CGFloat)MainWindowRenderScales()[(NSUInteger)index].doubleValue;
}

- (void)saveSnapshot:(id)sender
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.allowedContentTypes = @[ UTTypePNG ];
    panel.nameFieldStringValue = @"synesthesia-vibe-snapshot.png";
    __weak MainWindowController *weakSelf = self;
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse result) {
        if (result != NSModalResponseOK || panel.URL == nil)
            return;
        NSURL *url = panel.URL;
        [weakSelf.visualizerViewController saveSnapshotToURL:url completion:^(NSError *error) {
            if (error != nil)
                [weakSelf showError:error];
            else
                [weakSelf showStatus:[NSString stringWithFormat:@"Saved %@", url.lastPathComponent] isError:NO];
        }];
    }];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return (NSInteger)self.presetURLs.count;
}

- (nullable NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(nullable NSTableColumn *)tableColumn row:(NSInteger)row
{
    NSTableCellView *cell = [tableView makeViewWithIdentifier:MainWindowPresetColumn owner:self];
    if (cell == nil) {
        NSTextField *textField = [NSTextField labelWithString:@""];
        textField.lineBreakMode = NSLineBreakByTruncatingTail;
        textField.translatesAutoresizingMaskIntoConstraints = NO;
        cell = [[NSTableCellView alloc] initWithFrame:NSZeroRect];
        cell.identifier = MainWindowPresetColumn;
        cell.textField = textField;
        [cell addSubview:textField];
        [NSLayoutConstraint activateConstraints:@[
            [textField.leadingAnchor constraintEqualToAnchor:cell.leadingAnchor constant:2.0],
            [textField.trailingAnchor constraintEqualToAnchor:cell.trailingAnchor constant:-2.0],
            [textField.centerYAnchor constraintEqualToAnchor:cell.centerYAnchor]
        ]];
    }
    cell.textField.stringValue = self.presetURLs[(NSUInteger)row].URLByDeletingPathExtension.lastPathComponent;
    return cell;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSInteger row = self.presetTableView.selectedRow;
    if (row < 0 || (NSUInteger)row >= self.presetURLs.count)
        return;
    [self.visualizerViewController loadPresetAtURL:self.presetURLs[(NSUInteger)row]];
}

- (void)visualizerViewController:(VisualizerViewController *)controller didLoadPresetAtURL:(NSURL *)url
{
    self.window.subtitle = url.URLByDeletingPathExtension.lastPathComponent;
    [self showStatus:[NSString stringWithFormat:@"Loaded %@", url.URLByDeletingPathExtension.lastPathComponent] isError:NO];
}

- (void)visualizerViewController:(VisualizerViewController *)controller didFailWithError:(NSError *)error
{
    [self showError:error];
    [self syncAudioControls];
}

- (void)windowWillClose:(NSNotification *)notification
{
    [self.levelsTimer invalidate];
    self.levelsTimer = nil;
}

@end
