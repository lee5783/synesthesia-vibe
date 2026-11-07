//
//  SVPreparedPreset+Internal.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

#import "SVPreparedPreset.h"

#include <synesthesia-vibe/synesthesia-vibe.h>

NS_ASSUME_NONNULL_BEGIN

@interface SVPreparedPreset ()

@property (nonatomic, assign, readonly, nullable) sv_prepared_preset *rawPrepared;

- (instancetype)initWithRawPrepared:(sv_prepared_preset *)prepared NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
