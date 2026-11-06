//
//  SVPreparedPreset.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

#import "SVPreparedPreset+Internal.h"

@implementation SVPreparedPreset
{
    sv_prepared_preset* _prepared;
}

- (instancetype)initWithRawPrepared:(sv_prepared_preset *)prepared
{
    self = [super init];

    if (self != nil)
        _prepared = prepared;

    return self;
}

- (void)dealloc
{
    sv_prepared_preset_free(_prepared);
}

- (sv_prepared_preset *)rawPrepared
{
    return _prepared;
}

@end
