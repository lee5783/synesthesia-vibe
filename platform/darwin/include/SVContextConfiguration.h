//
//  SVContextConfiguration.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SVContextConfiguration : NSObject <NSCopying>

@property (nonatomic, assign) NSInteger width;
@property (nonatomic, assign) NSInteger height;
@property (nonatomic, assign) NSInteger meshX;
@property (nonatomic, assign) NSInteger meshY;
@property (nonatomic, assign) NSInteger texsizeX;
@property (nonatomic, assign) NSInteger texsizeY;
@property (nonatomic, assign) float fpsHint;

@property (nonatomic, copy, nullable) NSURL *resourceURL;
@property (nonatomic, copy, nullable) NSURL *textureSearchURL;
@property (nonatomic, copy, nullable) NSURL *cacheURL;

@property (nonatomic, assign) uint32_t rngSeed;
@property (nonatomic, assign, getter=isDeterministic) BOOL deterministic;

@property (nonatomic, assign) BOOL pinDate;
@property (nonatomic, assign) float year;
@property (nonatomic, assign) float month;
@property (nonatomic, assign) float day;
@property (nonatomic, assign) float weekday;
@property (nonatomic, assign) float hour;
@property (nonatomic, assign) float minute;
@property (nonatomic, assign) float second;
@property (nonatomic, assign) float milliseconds;

@property (nonatomic, assign) BOOL pinMouse;
@property (nonatomic, assign) float mouseX;
@property (nonatomic, assign) float mouseY;

+ (instancetype)captureStandardConfiguration;

@end

NS_ASSUME_NONNULL_END
