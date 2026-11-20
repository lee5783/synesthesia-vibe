//
//  AppDelegate.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "AppDelegate.h"

#import "MainWindowController.h"

@interface AppDelegate ()

@property (nonatomic, strong) MainWindowController *mainWindowController;

@end

@implementation AppDelegate

- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    NSApp.mainMenu = [self buildMainMenu];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    self.mainWindowController = [[MainWindowController alloc] init];
    [self.mainWindowController showWindow:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [self.mainWindowController applySyncLaunchDefaults];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app
{
    return YES;
}

- (NSMenu *)buildMainMenu
{
    NSString *applicationName = NSProcessInfo.processInfo.processName;

    NSMenu *applicationMenu = [[NSMenu alloc] initWithTitle:applicationName];
    [applicationMenu addItemWithTitle:[@"About " stringByAppendingString:applicationName]
                               action:@selector(orderFrontStandardAboutPanel:)
                        keyEquivalent:@""];
    [applicationMenu addItem:NSMenuItem.separatorItem];
    [applicationMenu addItemWithTitle:[@"Hide " stringByAppendingString:applicationName] action:@selector(hide:) keyEquivalent:@"h"];
    [applicationMenu addItemWithTitle:[@"Quit " stringByAppendingString:applicationName] action:@selector(terminate:) keyEquivalent:@"q"];

    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    NSMenuItem *fullScreenItem = [viewMenu addItemWithTitle:@"Enter Full Screen" action:@selector(toggleFullScreen:) keyEquivalent:@"f"];
    fullScreenItem.keyEquivalentModifierMask = NSEventModifierFlagControl | NSEventModifierFlagCommand;

    NSMenuItem *debugItem = [viewMenu addItemWithTitle:@"Debug Overlay" action:@selector(toggleDebugOverlay:) keyEquivalent:@"d"];
    debugItem.keyEquivalentModifierMask = NSEventModifierFlagShift | NSEventModifierFlagCommand;

    NSMenu *syncMenu = [[NSMenu alloc] initWithTitle:@"Sync"];
    NSMenuItem *mirrorItem = [syncMenu addItemWithTitle:@"New Mirror Window" action:@selector(newMirrorWindow:) keyEquivalent:@"n"];
    mirrorItem.keyEquivalentModifierMask = NSEventModifierFlagShift | NSEventModifierFlagCommand;
    NSMenuItem *syncItem = [syncMenu addItemWithTitle:@"Sync Visuals" action:@selector(toggleSyncVisuals:) keyEquivalent:@"y"];
    syncItem.keyEquivalentModifierMask = NSEventModifierFlagShift | NSEventModifierFlagCommand;
    [syncMenu addItem:NSMenuItem.separatorItem];
    NSArray<NSString *> *scalingTitles = @[ @"Scaling: Fill", @"Scaling: Fit", @"Scaling: Stretch" ];
    for (NSUInteger index = 0; index < scalingTitles.count; index++) {
        NSMenuItem *scalingItem = [syncMenu addItemWithTitle:scalingTitles[index] action:@selector(selectSyncScaling:) keyEquivalent:@""];
        scalingItem.tag = (NSInteger)index;
    }
    [syncMenu addItem:NSMenuItem.separatorItem];
    NSMenuItem *snapshotItem = [syncMenu addItemWithTitle:@"Save Sync Snapshots" action:@selector(saveSyncSnapshots:) keyEquivalent:@"s"];
    snapshotItem.keyEquivalentModifierMask = NSEventModifierFlagShift | NSEventModifierFlagCommand;
    NSMenuItem *orderItem = [syncMenu addItemWithTitle:@"Hide Main Window" action:@selector(toggleMainWindowOrderedOut:) keyEquivalent:@"o"];
    orderItem.keyEquivalentModifierMask = NSEventModifierFlagShift | NSEventModifierFlagCommand;
    for (NSMenuItem *item in syncMenu.itemArray)
        item.target = self;

    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
    NSApp.windowsMenu = windowMenu;

    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@""];
    for (NSMenu *menu in @[ applicationMenu, viewMenu, syncMenu, windowMenu ]) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:menu.title action:nil keyEquivalent:@""];
        item.submenu = menu;
        [mainMenu addItem:item];
    }
    return mainMenu;
}

- (void)newMirrorWindow:(id)sender
{
    [self.mainWindowController showMirrorWindow];
}

- (void)toggleSyncVisuals:(id)sender
{
    self.mainWindowController.syncOn = !self.mainWindowController.isSyncOn;
}

- (void)selectSyncScaling:(NSMenuItem *)sender
{
    self.mainWindowController.syncScaling = (SVSyncScaling)sender.tag;
}

- (void)saveSyncSnapshots:(id)sender
{
    NSURL *directory = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"svsync-snapshots"] isDirectory:YES];
    [self.mainWindowController saveSyncSnapshotsToDirectory:directory];
}

- (void)toggleMainWindowOrderedOut:(id)sender
{
    [self.mainWindowController toggleMainWindowOrderedOut];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if (menuItem.action == @selector(toggleSyncVisuals:))
        menuItem.state = self.mainWindowController.isSyncOn ? NSControlStateValueOn : NSControlStateValueOff;
    else if (menuItem.action == @selector(selectSyncScaling:))
        menuItem.state = menuItem.tag == self.mainWindowController.syncScaling ? NSControlStateValueOn : NSControlStateValueOff;
    else if (menuItem.action == @selector(toggleMainWindowOrderedOut:))
        menuItem.title = self.mainWindowController.window.isVisible ? @"Hide Main Window" : @"Show Main Window";
    return YES;
}

@end
