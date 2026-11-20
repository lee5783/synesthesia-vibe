//
//  MainWindowController.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <AppKit/AppKit.h>
@import SynesthesiaVibeCore;

NS_ASSUME_NONNULL_BEGIN

@interface MainWindowController : NSWindowController

@property (nonatomic, assign, getter=isSyncOn) BOOL syncOn;
@property (nonatomic, assign) SVSyncScaling syncScaling;

- (instancetype)init;
- (void)showMirrorWindow;
- (void)saveSyncSnapshotsToDirectory:(NSURL *)directory;
- (void)toggleMainWindowOrderedOut;
- (void)applySyncLaunchDefaults;

@end

NS_ASSUME_NONNULL_END
