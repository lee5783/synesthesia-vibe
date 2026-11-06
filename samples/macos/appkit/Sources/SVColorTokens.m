//
//  SVColorTokens.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-06.
//

#import "SVColorTokens.h"

@implementation SVColorTokens

+ (NSColor *)colorWithHex:(uint32_t)hex alpha:(CGFloat)alpha
{
    return [NSColor colorWithSRGBRed:(CGFloat)((hex >> 16) & 0xFF) / 255.0
                               green:(CGFloat)((hex >> 8) & 0xFF) / 255.0
                                blue:(CGFloat)(hex & 0xFF) / 255.0
                               alpha:alpha];
}

+ (NSColor *)colorWithHex:(uint32_t)hex
{
    return [self colorWithHex:hex alpha:1.0];
}

+ (NSColor *)backgroundPrimary { return [self colorWithHex:0x1e1e1e]; }
+ (NSColor *)backgroundSecondary { return [self colorWithHex:0x161616]; }
+ (NSColor *)surfaceCard { return [self colorWithHex:0x262626]; }
+ (NSColor *)surfaceElevated { return [self colorWithHex:0x2c2c2c]; }
+ (NSColor *)surfaceHover { return [self colorWithHex:0x313131]; }
+ (NSColor *)surfacePressed { return [self colorWithHex:0x383838]; }
+ (NSColor *)borderSubtle { return [self colorWithHex:0x333333]; }
+ (NSColor *)borderStrong { return [self colorWithHex:0x737373]; }
+ (NSColor *)textPrimary { return [self colorWithHex:0xdcddde]; }
+ (NSColor *)textSecondary { return [self colorWithHex:0xb4b4b6]; }
+ (NSColor *)textMuted { return [self colorWithHex:0x949494]; }
+ (NSColor *)textDisabled { return [self colorWithHex:0x5c5c5c]; }
+ (NSColor *)textOnAccent { return [self colorWithHex:0xffffff]; }
+ (NSColor *)accentPrimary { return [self colorWithHex:0x7c3aed]; }
+ (NSColor *)accentHover { return [self colorWithHex:0x6d28d9]; }
+ (NSColor *)accentPressed { return [self colorWithHex:0x5b21b6]; }
+ (NSColor *)accentText { return [self colorWithHex:0xa78bfa]; }
+ (NSColor *)accentSubtle { return [self colorWithHex:0x2a2140]; }
+ (NSColor *)overlay { return [self colorWithHex:0x000000 alpha:0.64]; }
+ (NSColor *)success { return [self colorWithHex:0x4ade80]; }
+ (NSColor *)warning { return [self colorWithHex:0xfbbf24]; }
+ (NSColor *)info { return [self colorWithHex:0x60a5fa]; }
+ (NSColor *)infoSubtle { return [self colorWithHex:0x16233a]; }
+ (NSColor *)danger { return [self colorWithHex:0xf87171]; }
+ (NSColor *)dangerSubtle { return [self colorWithHex:0x2f1818]; }
+ (NSColor *)letterbox { return [self colorWithHex:0x000000]; }
+ (NSColor *)debugCard { return [self colorWithHex:0x000000 alpha:0.85]; }
+ (NSColor *)hairline { return [self colorWithHex:0xffffff alpha:0.12]; }

@end
