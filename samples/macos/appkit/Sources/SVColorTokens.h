//
//  SVColorTokens.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SVColorTokens : NSObject

@property (class, nonatomic, readonly) NSColor *backgroundPrimary;
@property (class, nonatomic, readonly) NSColor *backgroundSecondary;
@property (class, nonatomic, readonly) NSColor *surfaceCard;
@property (class, nonatomic, readonly) NSColor *surfaceElevated;
@property (class, nonatomic, readonly) NSColor *surfaceHover;
@property (class, nonatomic, readonly) NSColor *surfacePressed;
@property (class, nonatomic, readonly) NSColor *borderSubtle;
@property (class, nonatomic, readonly) NSColor *borderStrong;
@property (class, nonatomic, readonly) NSColor *textPrimary;
@property (class, nonatomic, readonly) NSColor *textSecondary;
@property (class, nonatomic, readonly) NSColor *textMuted;
@property (class, nonatomic, readonly) NSColor *textDisabled;
@property (class, nonatomic, readonly) NSColor *textOnAccent;
@property (class, nonatomic, readonly) NSColor *accentPrimary;
@property (class, nonatomic, readonly) NSColor *accentHover;
@property (class, nonatomic, readonly) NSColor *accentPressed;
@property (class, nonatomic, readonly) NSColor *accentText;
@property (class, nonatomic, readonly) NSColor *accentSubtle;
@property (class, nonatomic, readonly) NSColor *overlay;
@property (class, nonatomic, readonly) NSColor *success;
@property (class, nonatomic, readonly) NSColor *warning;
@property (class, nonatomic, readonly) NSColor *info;
@property (class, nonatomic, readonly) NSColor *infoSubtle;
@property (class, nonatomic, readonly) NSColor *danger;
@property (class, nonatomic, readonly) NSColor *dangerSubtle;
@property (class, nonatomic, readonly) NSColor *letterbox;
@property (class, nonatomic, readonly) NSColor *debugCard;
@property (class, nonatomic, readonly) NSColor *hairline;

+ (NSColor *)colorWithHex:(uint32_t)hex;
+ (NSColor *)colorWithHex:(uint32_t)hex alpha:(CGFloat)alpha;

@end

NS_ASSUME_NONNULL_END
