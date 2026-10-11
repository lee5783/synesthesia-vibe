//
//  SVWavFeeder.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

#import "SVContext.h"

NS_ASSUME_NONNULL_BEGIN

@interface SVWavFeeder : NSObject

@property (nonatomic, copy, readonly) NSURL *URL;
@property (nonatomic, assign) NSUInteger samplesPerRenderFrame;
@property (nonatomic, assign, readonly) NSInteger sampleRate;
@property (nonatomic, assign, readonly) NSInteger channels;
@property (nonatomic, assign, readonly) NSUInteger frameCount;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (BOOL)feedNextFrameInto:(SVContext *)context NS_SWIFT_NAME(feedNextFrame(into:));

@end

NS_ASSUME_NONNULL_END
