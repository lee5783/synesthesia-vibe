//
//  CaptureCommand.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const CaptureCommandOption;

@interface CaptureCommand : NSObject

+ (int)runWithArguments:(NSArray<NSString *> *)arguments;

@end

NS_ASSUME_NONNULL_END
