//
//  SampleBundle.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SampleBundle : NSObject

@property (class, nonatomic, copy, readonly) NSURL *resourceURL;
@property (class, nonatomic, copy, readonly) NSArray<NSURL *> *presetURLs;
@property (class, nonatomic, copy, readonly) NSURL *defaultWavURL;

+ (nullable NSURL *)presetURLForCaptureIdentifier:(NSString *)identifier;

@end

NS_ASSUME_NONNULL_END
