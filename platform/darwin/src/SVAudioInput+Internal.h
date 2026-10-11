//
//  SVAudioInput+Internal.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <AVFoundation/AVFoundation.h>

#import "SVContext.h"

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXTERN AVAudioFormat *SVMakeTargetAudioFormat(void);
FOUNDATION_EXTERN void SVConvertAndPushPCM(SVContext *context, AVAudioConverter *converter, AVAudioFormat *targetFormat, AVAudioPCMBuffer *buffer, double ratio);

NS_ASSUME_NONNULL_END
