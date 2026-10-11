//
//  CaptureCommand.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "CaptureCommand.h"

#import <AppKit/AppKit.h>
@import SynesthesiaVibeCore;

#import "SampleBundle.h"

NSString *const CaptureCommandOption = @"--capture";

static NSString *const CaptureCommandWavOption = @"--wav";
static NSString *const CaptureCommandFramesOption = @"--frames";
static NSString *const CaptureCommandOutOption = @"--out";

static void CaptureCommandLog(NSString *message)
{
    fprintf(stderr, "AppKitSample: %s\n", message.UTF8String);
}

@implementation CaptureCommand

+ (int)runWithArguments:(NSArray<NSString *> *)arguments
{
    NSString *presetIdentifier = [self valueForOption:CaptureCommandOption inArguments:arguments];
    NSString *wavPath = [self valueForOption:CaptureCommandWavOption inArguments:arguments];
    NSString *framesText = [self valueForOption:CaptureCommandFramesOption inArguments:arguments];
    NSString *outPath = [self valueForOption:CaptureCommandOutOption inArguments:arguments];

    if (presetIdentifier == nil || outPath == nil) {
        CaptureCommandLog(@"usage: SynesthesiaVibeAppKit --capture <preset-id> [--wav <path>] [--frames 0,30,60,150,300,450,599] --out <dir>");
        return 2;
    }

    NSURL *presetURL = [SampleBundle presetURLForCaptureIdentifier:presetIdentifier];
    if (presetURL == nil) {
        CaptureCommandLog([NSString stringWithFormat:@"unknown preset '%@'", presetIdentifier]);
        return 2;
    }

    NSArray<NSNumber *> *frames = [self framesFromText:framesText ?: @"0,30,60,150,300,450,599"];
    if (frames == nil) {
        CaptureCommandLog([NSString stringWithFormat:@"invalid --frames '%@'", framesText]);
        return 2;
    }

    NSURL *wavURL = wavPath != nil ? [NSURL fileURLWithPath:wavPath] : SampleBundle.defaultWavURL;
    NSURL *outURL = [NSURL fileURLWithPath:outPath isDirectory:YES];

    NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    NSOpenGLContext *glContext = pixelFormat != nil ? [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil] : nil;
    if (glContext == nil) {
        CaptureCommandLog(@"could not create an OpenGL 3.2 Core context");
        return 1;
    }
    [glContext makeCurrentContext];

    SVContextConfiguration *configuration = [[SVContextConfiguration alloc] init];
    configuration.resourceURL = SampleBundle.resourceURL;
    configuration.cacheURL = [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];

    SVCaptureSession *session = [[SVCaptureSession alloc] initWithConfiguration:configuration
                                                                      presetURL:presetURL
                                                                         wavURL:wavURL
                                                                         frames:frames
                                                             outputDirectoryURL:outURL];
    CaptureCommandLog([NSString stringWithFormat:@"capture preset=%@ file=\"%@\" wav=\"%@\" frames=%@ out=\"%@\"",
                       presetIdentifier, presetURL.lastPathComponent, wavURL.path,
                       [frames componentsJoinedByString:@","], outURL.path]);

    NSError *error = nil;
    BOOL succeeded = [session runWithError:&error];
    for (NSURL *url in session.writtenFileURLs)
        CaptureCommandLog([NSString stringWithFormat:@"wrote %@", url.path]);

    [NSOpenGLContext clearCurrentContext];

    if (!succeeded) {
        CaptureCommandLog([NSString stringWithFormat:@"capture failed: %@", error.localizedDescription]);
        return 1;
    }

    CaptureCommandLog([NSString stringWithFormat:@"capture OK files=%lu", (unsigned long)session.writtenFileURLs.count]);
    return 0;
}

+ (nullable NSString *)valueForOption:(NSString *)option inArguments:(NSArray<NSString *> *)arguments
{
    NSUInteger index = [arguments indexOfObject:option];
    if (index == NSNotFound || index + 1 >= arguments.count)
        return nil;
    return arguments[index + 1];
}

+ (nullable NSArray<NSNumber *> *)framesFromText:(NSString *)text
{
    NSMutableArray<NSNumber *> *frames = [NSMutableArray array];
    for (NSString *component in [text componentsSeparatedByString:@","]) {
        NSScanner *scanner = [NSScanner scannerWithString:component];
        NSInteger frame = 0;
        if (![scanner scanInteger:&frame] || !scanner.isAtEnd || frame < 0)
            return nil;
        [frames addObject:@(frame)];
    }
    return frames.count > 0 ? frames : nil;
}

@end
