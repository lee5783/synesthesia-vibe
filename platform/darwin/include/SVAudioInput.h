//
//  SVAudioInput.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

#import "SVContext.h"

NS_ASSUME_NONNULL_BEGIN

@interface SVAudioInput : NSObject

@property (nonatomic, weak, readonly, nullable) SVContext *context;
@property (nonatomic, assign, readonly, getter=isRunning) BOOL running;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithContext:(SVContext *)context NS_DESIGNATED_INITIALIZER;

- (BOOL)startWithError:(NSError **)error NS_SWIFT_NAME(start());
- (void)stop;

@end

NS_ASSUME_NONNULL_END
