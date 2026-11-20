//
//  SVSyncGroup+Internal.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-20.
//

#import "SVSyncGroup.h"
#import "SVView.h"

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <OpenGLES/EAGL.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@interface SVSyncPresenter : NSObject

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (nullable instancetype)initWithGroup:(SVSyncGroup *)group error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (BOOL)presentIntoFramebuffer:(uint32_t)framebuffer pixelSize:(CGSize)pixelSize scaling:(SVSyncScaling)scaling;
- (void)invalidate;

@end

@interface SVSyncGroup (Internal)

- (void)addMember:(SVView *)view;
- (void)removeMember:(SVView *)view;
- (void)refreshMembersWithReason:(NSString *)reason;
- (void)renderFrameForMember:(SVView *)view;
#if TARGET_OS_IPHONE
- (nullable EAGLContext *)createPresentContext;
#endif

@end

@interface SVView (SyncGroupMember)

@property (nonatomic, assign, readonly, getter=isSyncMemberRendering) BOOL syncMemberRendering;

@end

NS_ASSUME_NONNULL_END
