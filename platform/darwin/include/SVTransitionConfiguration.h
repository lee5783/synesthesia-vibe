//
//  SVTransitionConfiguration.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SVPresetRequestReason) {
    SVPresetRequestReasonTimer = 0,
    SVPresetRequestReasonHardCut = 1
};

typedef NS_ENUM(NSInteger, SVBlendPattern) {
    SVBlendPatternRandom = -1,
    SVBlendPatternFade = 0,
    SVBlendPatternDirectionalWipe = 1,
    SVBlendPatternPlasma = 2,
    SVBlendPatternRadial = 3,
    SVBlendPatternClock = 4,
    SVBlendPatternSpiral = 5,
    SVBlendPatternDiamond = 6,
    SVBlendPatternNuclearClock = 7,
    SVBlendPatternSquare = 8,
    SVBlendPatternCheckerboardWipe = 9,
    SVBlendPatternCurtain = 10,
    SVBlendPatternBubble = 11,
    SVBlendPatternKaleidoscope = 12,
    SVBlendPatternMoebiusStrip = 13,
    SVBlendPatternStar = 14,
    SVBlendPatternDiscoFloor = 15,
    SVBlendPatternFire = 16,
    SVBlendPatternWhirlpoolDrain = 17,
    SVBlendPatternJuliaFractal = 18
};

FOUNDATION_EXPORT NSInteger const SVBlendPatternCount;

FOUNDATION_EXPORT NSString *SVBlendPatternName(NSInteger pattern) NS_SWIFT_NAME(SVBlendPattern.name(for:));

@interface SVTransitionConfiguration : NSObject <NSCopying>

@property (nonatomic, assign) float softCutSeconds;
@property (nonatomic, assign) float presetDuration;
@property (nonatomic, assign) float presetDurationRandom;
@property (nonatomic, assign) BOOL hardCutsEnabled;
@property (nonatomic, assign) float hardCutLoudness;
@property (nonatomic, assign) float hardCutHalfLife;

@end

@interface SVPresetRequest : NSObject

@property (nonatomic, assign, readonly) SVPresetRequestReason reason;
@property (nonatomic, assign, readonly) float suggestedBlendSeconds;

- (instancetype)initWithReason:(SVPresetRequestReason)reason suggestedBlendSeconds:(float)suggestedBlendSeconds NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
