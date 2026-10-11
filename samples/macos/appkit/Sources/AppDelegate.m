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

    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
    NSApp.windowsMenu = windowMenu;

    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@""];
    for (NSMenu *menu in @[ applicationMenu, viewMenu, windowMenu ]) {
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:menu.title action:nil keyEquivalent:@""];
        item.submenu = menu;
        [mainMenu addItem:item];
    }
    return mainMenu;
}

@end
