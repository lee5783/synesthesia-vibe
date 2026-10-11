//
//  SVContext+Internal.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVContext.h"

#include <synesthesia-vibe/synesthesia-vibe.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXTERN NSError *SVMakeError(NSInteger code, NSString *message);

@interface SVContext (Internal)

@property (nonatomic, assign, readonly, nullable) sv_context *rawContext;

@end

NS_ASSUME_NONNULL_END
