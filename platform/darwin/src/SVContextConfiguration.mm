//
//  SVContextConfiguration.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVContextConfiguration.h"
#import "SVCaptureSession.h"

namespace
{
    constexpr NSInteger kCaptureMeshX = 64;
    constexpr NSInteger kCaptureMeshY = 48;
    constexpr NSInteger kCaptureTexSizeX = 848;
    constexpr NSInteger kCaptureTexSizeY = 448;
    constexpr float kCaptureFps = 60.0f;
    constexpr float kPinnedYear = 2000.0f;
    constexpr float kPinnedMonth = 1.0f;
    constexpr float kPinnedDay = 1.0f;
    constexpr float kPinnedWeekday = 6.0f;
}

@implementation SVContextConfiguration

+ (instancetype)captureStandardConfiguration
{
    SVContextConfiguration *configuration = [[self alloc] init];
    configuration.width = SVCaptureWidth;
    configuration.height = SVCaptureHeight;
    configuration.meshX = kCaptureMeshX;
    configuration.meshY = kCaptureMeshY;
    configuration.texsizeX = kCaptureTexSizeX;
    configuration.texsizeY = kCaptureTexSizeY;
    configuration.fpsHint = kCaptureFps;
    configuration.rngSeed = SVCaptureRngSeed;
    configuration.deterministic = YES;
    configuration.pinDate = YES;
    configuration.year = kPinnedYear;
    configuration.month = kPinnedMonth;
    configuration.day = kPinnedDay;
    configuration.weekday = kPinnedWeekday;
    return configuration;
}

- (id)copyWithZone:(NSZone *)zone
{
    SVContextConfiguration *copy = [[[self class] allocWithZone:zone] init];
    copy.width = self.width;
    copy.height = self.height;
    copy.meshX = self.meshX;
    copy.meshY = self.meshY;
    copy.texsizeX = self.texsizeX;
    copy.texsizeY = self.texsizeY;
    copy.fpsHint = self.fpsHint;
    copy.resourceURL = self.resourceURL;
    copy.textureSearchURL = self.textureSearchURL;
    copy.cacheURL = self.cacheURL;
    copy.rngSeed = self.rngSeed;
    copy.deterministic = self.deterministic;
    copy.pinDate = self.pinDate;
    copy.year = self.year;
    copy.month = self.month;
    copy.day = self.day;
    copy.weekday = self.weekday;
    copy.hour = self.hour;
    copy.minute = self.minute;
    copy.second = self.second;
    copy.milliseconds = self.milliseconds;
    copy.pinMouse = self.pinMouse;
    copy.mouseX = self.mouseX;
    copy.mouseY = self.mouseY;
    return copy;
}

@end
