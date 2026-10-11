//
//  main.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import <AppKit/AppKit.h>

#import "AppDelegate.h"
#import "CaptureCommand.h"

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSArray<NSString *> *arguments = NSProcessInfo.processInfo.arguments;
        if ([arguments containsObject:CaptureCommandOption])
            return [CaptureCommand runWithArguments:arguments];

        NSApplication *application = NSApplication.sharedApplication;
        AppDelegate *delegate = [[AppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}
