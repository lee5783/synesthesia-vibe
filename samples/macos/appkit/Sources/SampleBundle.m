//
//  SampleBundle.m
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SampleBundle.h"

static NSString *const SampleBundlePresetsDirectory = @"presets";
static NSString *const SampleBundleCapturePresetsDirectory = @"capture-presets";
static NSString *const SampleBundleAudioDirectory = @"audio";
static NSString *const SampleBundleDefaultWavName = @"deterministic-test.wav";
static NSString *const SampleBundleCaptureSetName = @"capture-set.json";

@implementation SampleBundle

+ (NSURL *)resourceURL
{
    return NSBundle.mainBundle.resourceURL;
}

+ (NSURL *)presetsDirectoryURL
{
    return [self.resourceURL URLByAppendingPathComponent:SampleBundlePresetsDirectory isDirectory:YES];
}

+ (NSArray<NSURL *> *)presetURLs
{
    NSArray<NSURL *> *contents = [NSFileManager.defaultManager contentsOfDirectoryAtURL:self.presetsDirectoryURL
                                                             includingPropertiesForKeys:nil
                                                                                options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                                  error:nil];
    NSPredicate *milkFiles = [NSPredicate predicateWithFormat:@"pathExtension ==[c] 'milk'"];
    NSArray<NSURL *> *presets = [contents ?: @[] filteredArrayUsingPredicate:milkFiles];
    return [presets sortedArrayUsingComparator:^NSComparisonResult(NSURL *left, NSURL *right) {
        return [left.lastPathComponent localizedStandardCompare:right.lastPathComponent];
    }];
}

+ (NSURL *)defaultWavURL
{
    return [[self.resourceURL URLByAppendingPathComponent:SampleBundleAudioDirectory isDirectory:YES]
        URLByAppendingPathComponent:SampleBundleDefaultWavName];
}

+ (nullable NSURL *)presetURLForCaptureIdentifier:(NSString *)identifier
{
    NSData *data = [NSData dataWithContentsOfURL:[self.resourceURL URLByAppendingPathComponent:SampleBundleCaptureSetName]];
    NSDictionary *captureSet = data != nil ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
    NSArray *presets = [captureSet isKindOfClass:NSDictionary.class] ? captureSet[@"presets"] : nil;

    for (NSDictionary *preset in presets) {
        if ([preset isKindOfClass:NSDictionary.class] && [preset[@"id"] isEqual:identifier])
            return [[self.resourceURL URLByAppendingPathComponent:SampleBundleCapturePresetsDirectory isDirectory:YES]
                URLByAppendingPathComponent:preset[@"file"]];
    }

    for (NSURL *url in self.presetURLs) {
        if ([url.lastPathComponent isEqualToString:identifier] ||
            [url.URLByDeletingPathExtension.lastPathComponent isEqualToString:identifier])
            return url;
    }

    if ([NSFileManager.defaultManager fileExistsAtPath:identifier])
        return [NSURL fileURLWithPath:identifier];

    return nil;
}

@end
