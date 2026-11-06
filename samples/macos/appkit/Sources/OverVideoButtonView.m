//
//  OverVideoButtonView.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import "OverVideoButtonView.h"

#import "SVColorTokens.h"

static const CGFloat OverVideoButtonWidth = 32.0;
static const CGFloat OverVideoButtonHeight = 28.0;
static const CGFloat OverVideoButtonCornerRadius = 6.0;

@interface OverVideoButtonView ()

@property (nonatomic, strong) NSImageView *imageView;
@property (nonatomic, strong) NSTrackingArea *trackingArea;
@property (nonatomic, assign) BOOL isHighlighted;

@end

@implementation OverVideoButtonView

- (instancetype)initWithSymbolName:(NSString *)symbolName label:(NSString *)label toolTip:(nullable NSString *)toolTip
{
    self = [super initWithFrame:NSMakeRect(0.0, 0.0, OverVideoButtonWidth, OverVideoButtonHeight)];
    if (self) {
        _symbolName = [symbolName copy];
        self.toolTip = toolTip;
        self.accessibilityLabel = label;

        _imageView = [[NSImageView alloc] initWithFrame:NSZeroRect];
        _imageView.translatesAutoresizingMaskIntoConstraints = NO;
        _imageView.contentTintColor = SVColorTokens.textOnAccent;
        [self addSubview:_imageView];

        [NSLayoutConstraint activateConstraints:@[
            [self.widthAnchor constraintEqualToConstant:OverVideoButtonWidth],
            [self.heightAnchor constraintEqualToConstant:OverVideoButtonHeight],
            [_imageView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
            [_imageView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor]
        ]];

        [self updateSymbol];
    }
    return self;
}

- (BOOL)isOpaque
{
    return NO;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)setSymbolName:(NSString *)symbolName
{
    _symbolName = [symbolName copy];
    [self updateSymbol];
}

- (void)updateSymbol
{
    NSImage *image = [NSImage imageWithSystemSymbolName:self.symbolName accessibilityDescription:self.accessibilityLabel];
    self.imageView.image = image;
}

- (void)setOn:(BOOL)on
{
    _on = on;
    self.imageView.contentTintColor = on ? SVColorTokens.textOnAccent : SVColorTokens.textPrimary;
    self.needsDisplay = YES;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    if (self.trackingArea != nil)
        [self removeTrackingArea:self.trackingArea];
    self.trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                     options:NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
                                                       owner:self
                                                    userInfo:nil];
    [self addTrackingArea:self.trackingArea];
}

- (void)mouseEntered:(NSEvent *)event
{
    self.isHighlighted = YES;
    self.needsDisplay = YES;
}

- (void)mouseExited:(NSEvent *)event
{
    self.isHighlighted = NO;
    self.needsDisplay = YES;
}

- (void)mouseDown:(NSEvent *)event
{
    self.isHighlighted = YES;
    self.needsDisplay = YES;
}

- (void)mouseUp:(NSEvent *)event
{
    self.isHighlighted = NO;
    self.needsDisplay = YES;
    if (NSPointInRect([self convertPoint:event.locationInWindow fromView:nil], self.bounds))
        [self sendAction:self.action to:self.target];
}

- (void)drawRect:(NSRect)dirtyRect
{
    NSRect button = NSInsetRect(self.bounds, 0.5, 0.5);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:button
                                                        xRadius:OverVideoButtonCornerRadius
                                                        yRadius:OverVideoButtonCornerRadius];
    NSColor *fill = self.isOn ? SVColorTokens.accentPrimary : SVColorTokens.overlay;
    if (self.isHighlighted)
        fill = self.isOn ? SVColorTokens.accentHover : SVColorTokens.surfaceElevated;
    [fill setFill];
    [path fill];
    [SVColorTokens.hairline setStroke];
    path.lineWidth = 1.0;
    [path stroke];
}

@end
