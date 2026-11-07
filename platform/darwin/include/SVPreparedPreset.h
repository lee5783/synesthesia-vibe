//
//  SVPreparedPreset.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/* The CPU half of a preset load, built by -[SVContext preparePresetAtURL:error:] on any thread and committed once with -[SVContext commitPreparedPreset:blendSeconds:error:] on the render thread. It holds no GL objects; releasing it on any thread frees it, committed or not. */
@interface SVPreparedPreset : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
