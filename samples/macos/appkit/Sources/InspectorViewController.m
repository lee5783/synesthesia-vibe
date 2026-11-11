//
//  InspectorViewController.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import "InspectorViewController.h"

#import "SVColorTokens.h"

@import SynesthesiaVibeCore;

static const CGFloat InspectorWidth = 320.0;
static const CGFloat InspectorCardCornerRadius = 8.0;
static NSArray<NSNumber *> *InspectorFrameRates(void)
{
    return @[ @30, @60, @120, @0 ];
}

static NSArray<NSNumber *> *InspectorAutoIntervals(void)
{
    return @[ @10, @15, @30, @60 ];
}

static NSArray<NSNumber *> *InspectorBlendSeconds(void)
{
    return @[ @0.0f, @1.0f, @2.0f, @2.7f, @4.0f, @6.0f, @8.0f, @10.0f ];
}

static NSString *InspectorBlendSecondsTitle(float seconds)
{
    return seconds <= 0.0f ? @"Instant" : [NSString stringWithFormat:@"%g s", seconds];
}

static NSString *InspectorAutoIntervalTitle(double interval)
{
    return interval >= 60.0 ? [NSString stringWithFormat:@"%.0f minute", interval / 60.0] : [NSString stringWithFormat:@"%.0f seconds", interval];
}

static NSString *InspectorFrameRateTitle(float frameRate)
{
    return frameRate > 0.0f ? [NSString stringWithFormat:@"%.0f fps", frameRate] : @"Uncapped";
}

@interface InspectorCardView : NSView
@end

@implementation InspectorCardView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        self.wantsLayer = YES;
        self.layer.backgroundColor = SVColorTokens.surfaceCard.CGColor;
        self.layer.cornerRadius = InspectorCardCornerRadius;
        self.layer.borderWidth = 1.0;
        self.layer.borderColor = SVColorTokens.borderSubtle.CGColor;
    }
    return self;
}

@end

@interface InspectorViewController ()

@property (nonatomic, strong) NSSwitch *autoChangeSwitch;
@property (nonatomic, strong) NSPopUpButton *autoChangeIntervalPopUp;
@property (nonatomic, strong) NSPopUpButton *blendPatternPopUp;
@property (nonatomic, strong) NSPopUpButton *blendSecondsPopUp;
@property (nonatomic, strong) NSSwitch *hardCutsSwitch;
@property (nonatomic, strong) NSTextField *everyLabel;
@property (nonatomic, strong) NSArray<NSButton *> *sourceButtons;
@property (nonatomic, strong) NSPopUpButton *frameRatePopUp;
@property (nonatomic, strong) NSSwitch *debugSwitch;
@property (nonatomic, strong) NSMutableArray<NSNumber *> *frameRateOptions;

@end

@implementation InspectorViewController

- (instancetype)init
{
    self = [super initWithNibName:nil bundle:nil];
    if (self) {
        _systemAudioAvailable = YES;
        _frameRateOptions = [@[ @30, @60, @120, @0 ] mutableCopy];
    }
    return self;
}

- (void)loadView
{
    NSView *view = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, InspectorWidth, 640.0)];
    view.wantsLayer = YES;
    view.layer.backgroundColor = SVColorTokens.backgroundPrimary.CGColor;

    NSStackView *groups = [NSStackView stackViewWithViews:@[
        [self playlistGroup],
        [self transitionGroup],
        [self audioGroup],
        [self playbackGroup],
        [self developerGroup]
    ]];
    groups.orientation = NSUserInterfaceLayoutOrientationVertical;
    groups.alignment = NSLayoutAttributeLeading;
    groups.spacing = 16.0;
    groups.translatesAutoresizingMaskIntoConstraints = NO;

    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scrollView.drawsBackground = NO;
    scrollView.hasVerticalScroller = YES;
    scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    scrollView.documentView = groups;

    [view addSubview:scrollView];
    [NSLayoutConstraint activateConstraints:@[
        [scrollView.topAnchor constraintEqualToAnchor:view.topAnchor],
        [scrollView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
        [scrollView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
        [scrollView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
        [groups.leadingAnchor constraintEqualToAnchor:scrollView.contentView.leadingAnchor constant:16.0],
        [groups.topAnchor constraintEqualToAnchor:scrollView.contentView.topAnchor constant:16.0],
        [groups.widthAnchor constraintEqualToConstant:InspectorWidth - 32.0]
    ]];

    self.view = view;
}

- (NSStackView *)groupWithTitle:(NSString *)title card:(NSView *)card footer:(nullable NSString *)footer
{
    NSMutableArray<NSView *> *views = [NSMutableArray array];
    NSTextField *header = [NSTextField labelWithString:title];
    header.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightSemibold];
    header.textColor = SVColorTokens.textMuted;
    header.translatesAutoresizingMaskIntoConstraints = NO;
    [views addObject:header];
    [views addObject:card];

    if (footer.length > 0) {
        NSTextField *footerLabel = [NSTextField wrappingLabelWithString:footer];
        footerLabel.font = [NSFont systemFontOfSize:11.0];
        footerLabel.textColor = SVColorTokens.textMuted;
        footerLabel.translatesAutoresizingMaskIntoConstraints = NO;
        [views addObject:footerLabel];
        [footerLabel.widthAnchor constraintEqualToConstant:InspectorWidth - 32.0].active = YES;
    }

    NSStackView *group = [NSStackView stackViewWithViews:views];
    group.orientation = NSUserInterfaceLayoutOrientationVertical;
    group.alignment = NSLayoutAttributeLeading;
    group.spacing = 6.0;
    group.translatesAutoresizingMaskIntoConstraints = NO;
    [group.widthAnchor constraintEqualToConstant:InspectorWidth - 32.0].active = YES;
    return group;
}

- (NSView *)playlistGroup
{
    NSStackView *rows = [NSStackView stackViewWithViews:@[
        [self autoChangeRow],
        [self dividerView],
        [self intervalRow]
    ]];
    rows.orientation = NSUserInterfaceLayoutOrientationVertical;
    rows.alignment = NSLayoutAttributeLeading;
    rows.spacing = 0.0;
    rows.translatesAutoresizingMaskIntoConstraints = NO;
    rows.edgeInsets = NSEdgeInsetsZero;

    return [self groupWithTitle:@"Playlist"
                           card:[self cardWithContent:rows]
                         footer:@"The engine moves to the next preset when the interval ends. Click a preset in the sidebar to play it."];
}

- (NSView *)autoChangeRow
{
    NSTextField *label = [NSTextField labelWithString:@"Auto-change"];
    label.textColor = SVColorTokens.textPrimary;

    self.autoChangeSwitch = [[NSSwitch alloc] initWithFrame:NSZeroRect];
    self.autoChangeSwitch.target = self;
    self.autoChangeSwitch.action = @selector(toggleAutoChange:);
    self.autoChangeSwitch.state = NSControlStateValueOff;
    self.autoChangeSwitch.accessibilityLabel = @"Auto-change";
    [self.autoChangeSwitch setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];

    return [self rowWithLeading:label trailing:self.autoChangeSwitch];
}

- (NSView *)intervalRow
{
    self.everyLabel = [NSTextField labelWithString:@"Every"];
    self.everyLabel.textColor = SVColorTokens.textDisabled;

    self.autoChangeIntervalPopUp = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    for (NSNumber *interval in InspectorAutoIntervals())
        [self.autoChangeIntervalPopUp addItemWithTitle:InspectorAutoIntervalTitle(interval.doubleValue)];
    [self.autoChangeIntervalPopUp selectItemAtIndex:2];
    self.autoChangeIntervalPopUp.enabled = NO;
    self.autoChangeIntervalPopUp.target = self;
    self.autoChangeIntervalPopUp.action = @selector(changeAutoChangeInterval:);
    self.autoChangeIntervalPopUp.accessibilityLabel = @"Change every";

    return [self rowWithLeading:self.everyLabel trailing:self.autoChangeIntervalPopUp];
}

- (NSView *)transitionGroup
{
    NSTextField *patternLabel = [NSTextField labelWithString:@"Transition"];
    patternLabel.textColor = SVColorTokens.textPrimary;

    self.blendPatternPopUp = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.blendPatternPopUp addItemWithTitle:SVBlendPatternName(SVBlendPatternRandom)];
    for (NSInteger pattern = 0; pattern < SVBlendPatternCount; pattern++)
        [self.blendPatternPopUp addItemWithTitle:SVBlendPatternName(pattern)];
    self.blendPatternPopUp.target = self;
    self.blendPatternPopUp.action = @selector(changeBlendPattern:);
    self.blendPatternPopUp.accessibilityLabel = @"Transition";

    NSTextField *secondsLabel = [NSTextField labelWithString:@"Blend duration"];
    secondsLabel.textColor = SVColorTokens.textPrimary;

    self.blendSecondsPopUp = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    for (NSNumber *seconds in InspectorBlendSeconds())
        [self.blendSecondsPopUp addItemWithTitle:InspectorBlendSecondsTitle(seconds.floatValue)];
    [self.blendSecondsPopUp selectItemAtIndex:2];
    self.blendSecondsPopUp.target = self;
    self.blendSecondsPopUp.action = @selector(changeBlendSeconds:);
    self.blendSecondsPopUp.accessibilityLabel = @"Blend duration";

    NSTextField *hardCutsLabel = [NSTextField labelWithString:@"Hard cuts"];
    hardCutsLabel.textColor = SVColorTokens.textPrimary;

    self.hardCutsSwitch = [[NSSwitch alloc] initWithFrame:NSZeroRect];
    self.hardCutsSwitch.target = self;
    self.hardCutsSwitch.action = @selector(toggleHardCuts:);
    self.hardCutsSwitch.state = NSControlStateValueOff;
    self.hardCutsSwitch.accessibilityLabel = @"Hard cuts";
    [self.hardCutsSwitch setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];

    NSStackView *rows = [NSStackView stackViewWithViews:@[
        [self rowWithLeading:patternLabel trailing:self.blendPatternPopUp],
        [self dividerView],
        [self rowWithLeading:secondsLabel trailing:self.blendSecondsPopUp],
        [self dividerView],
        [self rowWithLeading:hardCutsLabel trailing:self.hardCutsSwitch]
    ]];
    rows.orientation = NSUserInterfaceLayoutOrientationVertical;
    rows.alignment = NSLayoutAttributeLeading;
    rows.spacing = 0.0;
    rows.translatesAutoresizingMaskIntoConstraints = NO;

    return [self groupWithTitle:@"Transitions"
                           card:[self cardWithContent:rows]
                         footer:@"A hard cut loads the next preset with no blend."];
}

- (NSView *)audioGroup
{
    NSMutableArray<NSView *> *rows = [NSMutableArray array];
    NSArray<NSString *> *titles = @[ @"None", @"Bundled WAV", @"Microphone", @"System sound" ];
    NSMutableArray<NSButton *> *buttons = [NSMutableArray array];
    for (NSUInteger index = 0; index < titles.count; index++) {
        NSButton *button = [NSButton radioButtonWithTitle:titles[index] target:self action:@selector(selectAudioSource:)];
        button.tag = (NSInteger)index;
        button.translatesAutoresizingMaskIntoConstraints = NO;
        [buttons addObject:button];
        [rows addObject:[self rowWithLeading:button trailing:NSView.new]];
    }
    self.sourceButtons = buttons;

    NSStackView *column = [NSStackView stackViewWithViews:rows];
    column.orientation = NSUserInterfaceLayoutOrientationVertical;
    column.alignment = NSLayoutAttributeLeading;
    column.spacing = 8.0;
    column.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *card = [self cardWithContent:column];
    return [self groupWithTitle:@"Audio" card:card footer:self.audioFooter];
}

- (NSString *)audioFooter
{
    if (self.audioReason.length > 0)
        return self.audioReason;
    return @"System sound excludes this app’s own audio.";
}

- (NSView *)playbackGroup
{
    NSTextField *frameRateLabel = [NSTextField labelWithString:@"Frame rate"];
    frameRateLabel.textColor = SVColorTokens.textPrimary;

    self.frameRatePopUp = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self rebuildFrameRateMenu];
    self.frameRatePopUp.target = self;
    self.frameRatePopUp.action = @selector(changeFrameRate:);

    NSView *frameRateRow = [self rowWithLeading:frameRateLabel trailing:self.frameRatePopUp];

    NSStackView *column = [NSStackView stackViewWithViews:@[ frameRateRow ]];
    column.orientation = NSUserInterfaceLayoutOrientationVertical;
    column.alignment = NSLayoutAttributeLeading;
    column.spacing = 0.0;
    column.translatesAutoresizingMaskIntoConstraints = NO;

    return [self groupWithTitle:@"Playback"
                           card:[self cardWithContent:column]
                         footer:@"Frame rate applies live through preferredFramesPerSecond."];
}

- (NSView *)developerGroup
{
    NSTextField *label = [NSTextField labelWithString:@"Debug overlay"];
    label.textColor = SVColorTokens.textPrimary;

    self.debugSwitch = [[NSSwitch alloc] initWithFrame:NSZeroRect];
    self.debugSwitch.target = self;
    self.debugSwitch.action = @selector(toggleDebugOverlay:);
    self.debugSwitch.state = NSControlStateValueOff;
    self.debugSwitch.accessibilityLabel = @"Debug overlay";
    [self.debugSwitch setContentHuggingPriority:NSLayoutPriorityRequired forOrientation:NSLayoutConstraintOrientationHorizontal];

    NSView *card = [self cardWithContent:[self rowWithLeading:label trailing:self.debugSwitch]];
    return [self groupWithTitle:@"Developer" card:card footer:@"Stays on in full screen. ⇧⌘D toggles it."];
}

- (NSView *)cardWithContent:(NSView *)content
{
    InspectorCardView *card = [[InspectorCardView alloc] initWithFrame:NSZeroRect];
    card.translatesAutoresizingMaskIntoConstraints = NO;
    content.translatesAutoresizingMaskIntoConstraints = NO;
    [card addSubview:content];
    [NSLayoutConstraint activateConstraints:@[
        [card.widthAnchor constraintEqualToConstant:InspectorWidth - 32.0],
        [content.topAnchor constraintEqualToAnchor:card.topAnchor],
        [content.leadingAnchor constraintEqualToAnchor:card.leadingAnchor],
        [content.trailingAnchor constraintEqualToAnchor:card.trailingAnchor],
        [content.bottomAnchor constraintEqualToAnchor:card.bottomAnchor]
    ]];
    return card;
}

- (NSView *)rowWithLeading:(NSView *)leading trailing:(NSView *)trailing
{
    NSView *spacer = [[NSView alloc] initWithFrame:NSZeroRect];
    spacer.translatesAutoresizingMaskIntoConstraints = NO;
    [spacer setContentHuggingPriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    [spacer.widthAnchor constraintGreaterThanOrEqualToConstant:8.0].active = YES;

    NSStackView *row = [NSStackView stackViewWithViews:@[ leading, spacer, trailing ]];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.distribution = NSStackViewDistributionFill;
    row.spacing = 8.0;
    row.translatesAutoresizingMaskIntoConstraints = NO;
    row.edgeInsets = NSEdgeInsetsMake(10.0, 12.0, 10.0, 12.0);
    [row.widthAnchor constraintEqualToConstant:InspectorWidth - 32.0].active = YES;
    return row;
}

- (NSView *)dividerView
{
    NSView *divider = [[NSView alloc] initWithFrame:NSZeroRect];
    divider.wantsLayer = YES;
    divider.layer.backgroundColor = SVColorTokens.borderSubtle.CGColor;
    divider.translatesAutoresizingMaskIntoConstraints = NO;
    [divider.heightAnchor constraintEqualToConstant:1.0].active = YES;
    [divider.widthAnchor constraintEqualToConstant:InspectorWidth - 32.0].active = YES;
    return divider;
}

- (void)rebuildFrameRateMenu
{
    [self.frameRatePopUp removeAllItems];
    for (NSNumber *rate in self.frameRateOptions)
        [self.frameRatePopUp addItemWithTitle:InspectorFrameRateTitle(rate.floatValue)];
}

- (void)toggleAutoChange:(NSSwitch *)sender
{
    [self.delegate inspectorViewController:self didSetAutoChange:sender.state == NSControlStateValueOn];
}

- (void)changeAutoChangeInterval:(NSPopUpButton *)sender
{
    NSInteger index = sender.indexOfSelectedItem;
    if (index < 0 || (NSUInteger)index >= InspectorAutoIntervals().count)
        return;
    [self.delegate inspectorViewController:self didSetAutoChangeInterval:InspectorAutoIntervals()[(NSUInteger)index].doubleValue];
}

- (void)changeBlendPattern:(NSPopUpButton *)sender
{
    [self.delegate inspectorViewController:self didSelectBlendPattern:sender.indexOfSelectedItem - 1];
}

- (void)changeBlendSeconds:(NSPopUpButton *)sender
{
    NSInteger index = sender.indexOfSelectedItem;
    if (index < 0 || (NSUInteger)index >= InspectorBlendSeconds().count)
        return;
    [self.delegate inspectorViewController:self didSetBlendSeconds:InspectorBlendSeconds()[(NSUInteger)index].floatValue];
}

- (void)toggleHardCuts:(NSSwitch *)sender
{
    [self.delegate inspectorViewController:self didSetHardCuts:sender.state == NSControlStateValueOn];
}

- (void)selectAudioSource:(NSButton *)sender
{
    [self.delegate inspectorViewController:self didSelectAudioSourceIndex:sender.tag];
}

- (void)changeFrameRate:(NSPopUpButton *)sender
{
    NSInteger index = sender.indexOfSelectedItem;
    if (index < 0 || (NSUInteger)index >= self.frameRateOptions.count)
        return;
    [self.delegate inspectorViewController:self didSetFrameRate:self.frameRateOptions[(NSUInteger)index].floatValue];
}

- (void)toggleDebugOverlay:(NSSwitch *)sender
{
    [self.delegate inspectorViewController:self didSetDebugOverlay:sender.state == NSControlStateValueOn];
}

- (void)setAutoChangeOn:(BOOL)on interval:(double)interval
{
    self.autoChangeSwitch.state = on ? NSControlStateValueOn : NSControlStateValueOff;
    self.everyLabel.textColor = on ? SVColorTokens.textPrimary : SVColorTokens.textDisabled;
    self.autoChangeIntervalPopUp.enabled = on;
    NSUInteger index = [InspectorAutoIntervals() indexOfObject:@(interval)];
    if (index != NSNotFound)
        [self.autoChangeIntervalPopUp selectItemAtIndex:(NSInteger)index];
}

- (void)setBlendPattern:(NSInteger)pattern blendSeconds:(float)seconds hardCutsOn:(BOOL)hardCutsOn
{
    [self.blendPatternPopUp selectItemAtIndex:pattern + 1];
    NSUInteger index = [InspectorBlendSeconds() indexOfObject:@(seconds)];
    if (index != NSNotFound)
        [self.blendSecondsPopUp selectItemAtIndex:(NSInteger)index];
    self.hardCutsSwitch.state = hardCutsOn ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)setFrameRate:(float)frameRate
{
    if (![self.frameRateOptions containsObject:@(frameRate)]) {
        [self.frameRateOptions addObject:@(frameRate)];
        [self rebuildFrameRateMenu];
    }
    NSUInteger index = [self.frameRateOptions indexOfObject:@(frameRate)];
    if (index != NSNotFound)
        [self.frameRatePopUp selectItemAtIndex:(NSInteger)index];
}

- (void)setAudioSourceIndex:(NSInteger)index
{
    for (NSButton *button in self.sourceButtons)
        button.state = button.tag == index ? NSControlStateValueOn : NSControlStateValueOff;
    [self applySourceAvailability];
}

- (void)setDebugOverlayOn:(BOOL)on
{
    self.debugSwitch.state = on ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)setSystemAudioAvailable:(BOOL)systemAudioAvailable
{
    _systemAudioAvailable = systemAudioAvailable;
    [self applySourceAvailability];
}

- (void)setAudioReason:(nullable NSString *)audioReason
{
    _audioReason = [audioReason copy];
    self.view.accessibilityHelp = _audioReason;
}

- (void)applySourceAvailability
{
    if (self.sourceButtons.count < 4)
        return;
    NSButton *systemButton = self.sourceButtons[3];
    systemButton.enabled = self.systemAudioAvailable;
    systemButton.toolTip = self.systemAudioAvailable ? nil : @"Needs macOS 14.2 or later";
}

@end
