//
//  MirrorWindowController.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

#import <AppKit/AppKit.h>
@import SynesthesiaVibeCore;

NS_ASSUME_NONNULL_BEGIN

@interface MirrorWindowController : NSWindowController

@property (nonatomic, strong, readonly) SVView *visualizerView;

- (instancetype)initWithConfiguration:(SVContextConfiguration *)configuration
                            presetURL:(nullable NSURL *)presetURL
                          contentSize:(NSSize)contentSize;
- (void)matchContentSize:(NSSize)contentSize;

@end

NS_ASSUME_NONNULL_END
