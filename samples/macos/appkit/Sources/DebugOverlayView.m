//
//  DebugOverlayView.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import "DebugOverlayView.h"

#import "SVColorTokens.h"

static const CGFloat DebugOverlayWidth = 220.0;
static const CGFloat DebugOverlayLabelColumn = 52.0;
static const CGFloat DebugOverlayBarLabelWidth = 34.0;
static const CGFloat DebugOverlayBarHeight = 6.0;
static const CGFloat DebugOverlayCornerRadius = 8.0;

@interface DebugOverlayBarView : NSView

@property (nonatomic, strong) NSColor *barColor;
@property (nonatomic, assign) CGFloat value;

@end

@implementation DebugOverlayBarView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self)
        _barColor = SVColorTokens.accentText;
    return self;
}

- (void)setValue:(CGFloat)value
{
    _value = MIN(MAX(value, 0.0), 1.0);
    self.needsDisplay = YES;
}

- (void)setBarColor:(NSColor *)barColor
{
    _barColor = barColor;
    self.needsDisplay = YES;
}

- (void)drawRect:(NSRect)dirtyRect
{
    NSRect track = NSInsetRect(self.bounds, 0.0, floor((NSHeight(self.bounds) - DebugOverlayBarHeight) / 2.0));
    NSBezierPath *trackPath = [NSBezierPath bezierPathWithRoundedRect:track xRadius:DebugOverlayBarHeight / 2.0 yRadius:DebugOverlayBarHeight / 2.0];
    [SVColorTokens.hairline setFill];
    [trackPath fill];

    if (self.value <= 0.0)
        return;

    NSRect fill = track;
    fill.size.width = NSWidth(track) * self.value;
    NSBezierPath *fillPath = [NSBezierPath bezierPathWithRoundedRect:fill xRadius:DebugOverlayBarHeight / 2.0 yRadius:DebugOverlayBarHeight / 2.0];
    [self.barColor setFill];
    [fillPath fill];
}

@end

@interface DebugOverlayView ()

@property (nonatomic, strong) NSTextField *fpsLabel;
@property (nonatomic, strong) NSTextField *capLabel;
@property (nonatomic, strong) NSTextField *frameMillisecondsLabel;
@property (nonatomic, strong) NSGridView *grid;
@property (nonatomic, strong) NSMutableArray<NSTextField *> *valueFields;
@property (nonatomic, strong) DebugOverlayBarView *bassBar;
@property (nonatomic, strong) DebugOverlayBarView *midBar;
@property (nonatomic, strong) DebugOverlayBarView *trebleBar;

@end

@implementation DebugOverlayView

static NSTextField *DebugOverlayTextField(NSColor *color, BOOL emphasized)
{
    NSTextField *field = [NSTextField labelWithString:@""];
    field.font = emphasized ? [NSFont monospacedSystemFontOfSize:18.0 weight:NSFontWeightSemibold]
                            : [NSFont monospacedSystemFontOfSize:11.0 weight:NSFontWeightRegular];
    field.textColor = color;
    field.lineBreakMode = NSLineBreakByTruncatingTail;
    field.translatesAutoresizingMaskIntoConstraints = NO;
    return field;
}

static DebugOverlayBarView *DebugOverlayBar(NSColor *color)
{
    DebugOverlayBarView *bar = [[DebugOverlayBarView alloc] initWithFrame:NSZeroRect];
    bar.barColor = color;
    bar.translatesAutoresizingMaskIntoConstraints = NO;
    [bar.heightAnchor constraintEqualToConstant:DebugOverlayBarHeight].active = YES;
    return bar;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self)
        [self buildContent];
    return self;
}

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect
{
    NSRect card = NSInsetRect(self.bounds, 0.5, 0.5);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:card
                                                        xRadius:DebugOverlayCornerRadius
                                                        yRadius:DebugOverlayCornerRadius];
    [SVColorTokens.debugCard setFill];
    [path fill];
    [SVColorTokens.hairline setStroke];
    path.lineWidth = 1.0;
    [path stroke];
}

- (void)buildContent
{
    self.fpsLabel = DebugOverlayTextField(SVColorTokens.success, YES);
    self.capLabel = DebugOverlayTextField(SVColorTokens.textMuted, NO);
    self.frameMillisecondsLabel = DebugOverlayTextField(SVColorTokens.textMuted, NO);

    NSStackView *header = [NSStackView stackViewWithViews:@[ self.fpsLabel, self.capLabel, NSView.new, self.frameMillisecondsLabel ]];
    header.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    header.alignment = NSLayoutAttributeFirstBaseline;
    header.spacing = 6.0;
    header.translatesAutoresizingMaskIntoConstraints = NO;

    self.grid = [[NSGridView alloc] initWithFrame:NSZeroRect];
    self.grid.translatesAutoresizingMaskIntoConstraints = NO;
    self.grid.rowSpacing = 1.0;
    self.grid.columnSpacing = 8.0;

    self.valueFields = [NSMutableArray array];
    for (NSString *title in @[ @"preset", @"size", @"gl", @"audio", @"rate", @"auto" ]) {
        NSTextField *label = DebugOverlayTextField(SVColorTokens.textMuted, NO);
        label.stringValue = title;
        [label.widthAnchor constraintEqualToConstant:DebugOverlayLabelColumn].active = YES;
        NSTextField *value = DebugOverlayTextField(SVColorTokens.textPrimary, NO);
        [self.grid addRowWithViews:@[ label, value ]];
        [self.valueFields addObject:value];
    }

    self.bassBar = DebugOverlayBar(SVColorTokens.accentText);
    self.midBar = DebugOverlayBar(SVColorTokens.info);
    self.trebleBar = DebugOverlayBar(SVColorTokens.success);

    NSStackView *bars = [NSStackView stackViewWithViews:@[
        [self barRowWithLabel:@"bass" bar:self.bassBar],
        [self barRowWithLabel:@"mid" bar:self.midBar],
        [self barRowWithLabel:@"treb" bar:self.trebleBar]
    ]];
    bars.orientation = NSUserInterfaceLayoutOrientationVertical;
    bars.alignment = NSLayoutAttributeLeading;
    bars.spacing = 3.0;
    bars.translatesAutoresizingMaskIntoConstraints = NO;

    NSStackView *content = [NSStackView stackViewWithViews:@[ header, self.grid, bars ]];
    content.orientation = NSUserInterfaceLayoutOrientationVertical;
    content.alignment = NSLayoutAttributeLeading;
    content.spacing = 6.0;
    content.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:content];
    [NSLayoutConstraint activateConstraints:@[
        [self.widthAnchor constraintEqualToConstant:DebugOverlayWidth],
        [content.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:12.0],
        [content.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-12.0],
        [content.topAnchor constraintEqualToAnchor:self.topAnchor constant:10.0],
        [content.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-10.0]
    ]];

    self.fpsText = @"0.0";
    self.capText = @"60";
    self.frameMillisecondsText = @"0.0";
    self.presetText = @"";
    self.sizeText = @"";
    self.backendText = @"";
    self.audioText = @"";
    self.rateText = @"1.0× · Blend";
    self.autoText = @"off";
}

- (NSView *)barRowWithLabel:(NSString *)title bar:(DebugOverlayBarView *)bar
{
    NSTextField *label = DebugOverlayTextField(SVColorTokens.textMuted, NO);
    label.stringValue = title;
    [label.widthAnchor constraintEqualToConstant:DebugOverlayBarLabelWidth].active = YES;

    NSStackView *row = [NSStackView stackViewWithViews:@[ label, bar ]];
    row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    row.alignment = NSLayoutAttributeCenterY;
    row.spacing = 8.0;
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [row.widthAnchor constraintEqualToConstant:DebugOverlayWidth - 24.0].active = YES;
    return row;
}

- (void)setFpsText:(NSString *)fpsText
{
    _fpsText = [fpsText copy];
    self.fpsLabel.stringValue = _fpsText;
}

- (void)setFpsIsHealthy:(BOOL)fpsIsHealthy
{
    _fpsIsHealthy = fpsIsHealthy;
    self.fpsLabel.textColor = fpsIsHealthy ? SVColorTokens.success : SVColorTokens.warning;
}

- (void)setCapText:(NSString *)capText
{
    _capText = [capText copy];
    self.capLabel.stringValue = [NSString stringWithFormat:@"fps · cap %@", _capText];
}

- (void)setFrameMillisecondsText:(NSString *)frameMillisecondsText
{
    _frameMillisecondsText = [frameMillisecondsText copy];
    self.frameMillisecondsLabel.stringValue = [NSString stringWithFormat:@"%@ ms", _frameMillisecondsText];
}

- (void)setPresetText:(NSString *)presetText
{
    _presetText = [presetText copy];
    self.valueFields[0].stringValue = _presetText;
}

- (void)setSizeText:(NSString *)sizeText
{
    _sizeText = [sizeText copy];
    self.valueFields[1].stringValue = _sizeText;
}

- (void)setBackendText:(NSString *)backendText
{
    _backendText = [backendText copy];
    self.valueFields[2].stringValue = _backendText;
}

- (void)setAudioText:(NSString *)audioText
{
    _audioText = [audioText copy];
    self.valueFields[3].stringValue = _audioText;
}

- (void)setRateText:(NSString *)rateText
{
    _rateText = [rateText copy];
    self.valueFields[4].stringValue = _rateText;
}

- (void)setAutoText:(NSString *)autoText
{
    _autoText = [autoText copy];
    self.valueFields[5].stringValue = _autoText;
}

- (void)setLastErrorText:(nullable NSString *)lastErrorText
{
    NSString *text = lastErrorText.length > 0 ? lastErrorText : @"";
    if ([text isEqualToString:(_lastErrorText ?: @"")])
        return;

    _lastErrorText = [text copy];
    if (self.grid.numberOfRows > self.valueFields.count)
        [self.grid removeRowAtIndex:self.valueFields.count];

    if (text.length == 0)
        return;

    NSTextField *label = DebugOverlayTextField(SVColorTokens.textMuted, NO);
    label.stringValue = @"error";
    [label.widthAnchor constraintEqualToConstant:DebugOverlayLabelColumn].active = YES;
    NSTextField *value = DebugOverlayTextField(SVColorTokens.danger, NO);
    value.stringValue = text;
    [self.grid addRowWithViews:@[ label, value ]];
}

- (void)setBass:(float)bass
{
    _bass = bass;
    self.bassBar.value = bass / 3.0f;
}

- (void)setMid:(float)mid
{
    _mid = mid;
    self.midBar.value = mid / 3.0f;
}

- (void)setTreble:(float)treble
{
    _treble = treble;
    self.trebleBar.value = treble / 3.0f;
}

@end
