//
//  SVAudioInput+Internal.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <AVFoundation/AVFoundation.h>

#import "SVAudioInput.h"
#import "SVContext.h"

NS_ASSUME_NONNULL_BEGIN

@interface SVAudioPCMSink : NSObject

@property (atomic, assign) float gain;
@property (atomic, copy, nullable) SVAudioPCMHandler handler;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (instancetype)initWithContext:(nullable SVContext *)context NS_DESIGNATED_INITIALIZER;

- (void)pushFloat:(float *)interleaved frames:(NSUInteger)frames channels:(NSInteger)channels;

@end

@interface SVAudioConversion : NSObject

@property (nonatomic, strong, readonly) AVAudioFormat *inputFormat;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

- (nullable instancetype)initWithInputFormat:(AVAudioFormat *)inputFormat maximumInputFrames:(AVAudioFrameCount)maximumInputFrames NS_DESIGNATED_INITIALIZER;

- (void)convertBuffer:(AVAudioPCMBuffer *)buffer intoSink:(SVAudioPCMSink *)sink;
- (void)convertBufferList:(const AudioBufferList *)bufferList intoSink:(SVAudioPCMSink *)sink;

@end

FOUNDATION_EXTERN AVAudioFormat *SVMakeTargetAudioFormat(void);
FOUNDATION_EXTERN void SVConvertAndPushPCM(SVAudioPCMSink *sink, AVAudioConverter *converter, AVAudioConverterInputBlock inputBlock, AVAudioPCMBuffer *converted);
FOUNDATION_EXTERN void SVRegisterAudioSinkContext(SVContext *context);
FOUNDATION_EXTERN float SVNormalizedInputGain(float gain);

NS_ASSUME_NONNULL_END
