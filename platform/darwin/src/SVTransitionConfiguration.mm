//
//  SVTransitionConfiguration.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

#import "SVTransitionConfiguration.h"

NSInteger const SVBlendPatternCount = 19;

namespace
{
    constexpr float kDefaultSoftCutSeconds = 2.7f;
    constexpr float kDefaultPresetDuration = 16.0f;
    constexpr float kDefaultPresetDurationRandom = 10.0f;
    constexpr float kDefaultHardCutLoudness = 2.5f;
    constexpr float kDefaultHardCutHalfLife = 60.0f;
}

NSString *SVBlendPatternName(NSInteger pattern)
{
    static NSArray<NSString *> *names;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        names = @[
            @"Fade", @"Directional Wipe", @"Plasma", @"Radial", @"Clock", @"Spiral", @"Diamond",
            @"Nuclear Clock", @"Square", @"Checkerboard Wipe", @"Curtain", @"Bubble", @"Kaleidoscope",
            @"Moebius Strip", @"Star", @"Disco Floor", @"Fire", @"Whirlpool Drain", @"Julia Fractal"
        ];
    });

    if (pattern < 0 || pattern >= SVBlendPatternCount)
        return @"Random";

    return names[static_cast<NSUInteger>(pattern)];
}

@implementation SVTransitionConfiguration

- (instancetype)init
{
    self = [super init];

    if (self == nil)
        return nil;

    _softCutSeconds = kDefaultSoftCutSeconds;
    _presetDuration = kDefaultPresetDuration;
    _presetDurationRandom = kDefaultPresetDurationRandom;
    _hardCutsEnabled = NO;
    _hardCutLoudness = kDefaultHardCutLoudness;
    _hardCutHalfLife = kDefaultHardCutHalfLife;
    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    SVTransitionConfiguration *copy = [[[self class] allocWithZone:zone] init];
    copy.softCutSeconds = self.softCutSeconds;
    copy.presetDuration = self.presetDuration;
    copy.presetDurationRandom = self.presetDurationRandom;
    copy.hardCutsEnabled = self.hardCutsEnabled;
    copy.hardCutLoudness = self.hardCutLoudness;
    copy.hardCutHalfLife = self.hardCutHalfLife;
    return copy;
}

@end

@implementation SVPresetRequest

- (instancetype)initWithReason:(SVPresetRequestReason)reason suggestedBlendSeconds:(float)suggestedBlendSeconds
{
    self = [super init];

    if (self == nil)
        return nil;

    _reason = reason;
    _suggestedBlendSeconds = suggestedBlendSeconds;
    return self;
}

@end
