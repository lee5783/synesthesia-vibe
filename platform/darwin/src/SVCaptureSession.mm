//
//  SVCaptureSession.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVCaptureSession.h"
#import "SVContext+Internal.h"
#import "SVWavFeeder.h"

#include "capture.h"
#include "framebuffer.h"
#include "gl_headers.h"

#include <vector>

const NSInteger SVCaptureWidth = 838;
const NSInteger SVCaptureHeight = 441;
const uint32_t SVCaptureRngSeed = 20260927;

namespace
{
    constexpr NSTimeInterval kFrameSeconds = 1.0 / 60.0;
    constexpr NSUInteger kSamplesPerRenderFrame = 735;
    constexpr NSInteger kSilenceChannels = 2;
    constexpr float kClearRed = 51.0f / 255.0f;
    constexpr float kClearGreen = 102.0f / 255.0f;
    constexpr float kClearBlue = 153.0f / 255.0f;
    constexpr float kClearAlpha = 1.0f;
}

@interface SVCaptureSession ()

@property (nonatomic, copy, readwrite) NSArray<NSURL *> *writtenFileURLs;

@end

@implementation SVCaptureSession

- (instancetype)initWithConfiguration:(SVContextConfiguration *)configuration
                            presetURL:(NSURL *)presetURL
                               wavURL:(nullable NSURL *)wavURL
                               frames:(NSArray<NSNumber *> *)frames
                   outputDirectoryURL:(NSURL *)outputDirectoryURL
{
    self = [super init];

    if (self == nil)
        return nil;

    SVContextConfiguration *standard = [SVContextConfiguration captureStandardConfiguration];
    standard.resourceURL = configuration.resourceURL;
    standard.textureSearchURL = configuration.textureSearchURL;
    standard.cacheURL = configuration.cacheURL;

    if (configuration.pinDate)
    {
        standard.year = configuration.year;
        standard.month = configuration.month;
        standard.day = configuration.day;
        standard.weekday = configuration.weekday;
        standard.hour = configuration.hour;
        standard.minute = configuration.minute;
        standard.second = configuration.second;
        standard.milliseconds = configuration.milliseconds;
    }

    standard.pinMouse = configuration.pinMouse;
    standard.mouseX = configuration.mouseX;
    standard.mouseY = configuration.mouseY;

    _configuration = standard;
    _presetURL = [presetURL copy];
    _wavURL = [wavURL copy];
    _frames = [[frames sortedArrayUsingSelector:@selector(compare:)] copy];
    _outputDirectoryURL = [outputDirectoryURL copy];
    _writesFrameState = YES;
    _writtenFileURLs = @[];
    return self;
}

- (NSURL *)fileURLForFrame:(unsigned)frame extension:(NSString *)extension
{
    NSString *name = [NSString stringWithFormat:@"frame_%04u.%@", frame, extension];
    return [self.outputDirectoryURL URLByAppendingPathComponent:name isDirectory:NO];
}

- (BOOL)runWithError:(NSError **)error
{
    if (self.frames.count == 0)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorArgument, @"capture frame list is empty");
        return NO;
    }

    if (![[NSFileManager defaultManager] createDirectoryAtURL:self.outputDirectoryURL withIntermediateDirectories:YES attributes:nil error:error])
        return NO;

    SVWavFeeder *feeder = nil;

    if (self.wavURL != nil)
    {
        feeder = [[SVWavFeeder alloc] initWithURL:self.wavURL error:error];

        if (feeder == nil)
            return NO;

        feeder.samplesPerRenderFrame = kSamplesPerRenderFrame;
    }

    SVContext *context = [[SVContext alloc] initWithConfiguration:self.configuration error:error];

    if (context == nil)
        return NO;

    if (![context loadPresetAtURL:self.presetURL blendSeconds:0.0f error:error])
    {
        [context invalidate];
        return NO;
    }

    NSMutableSet<NSNumber *> *wanted = [NSMutableSet setWithArray:self.frames];
    const unsigned lastFrame = self.frames.lastObject.unsignedIntValue;
    NSMutableArray<NSURL *> *written = [NSMutableArray array];
    BOOL ok = YES;

    {
        render_gl::Framebuffer framebuffer;

        if (!framebuffer.Create(static_cast<int>(SVCaptureWidth), static_cast<int>(SVCaptureHeight)))
        {
            [context invalidate];

            if (error != nullptr)
                *error = SVMakeError(SVErrorGL, @"capture framebuffer could not be created");
            return NO;
        }

        framebuffer.Bind();
        glClearColor(kClearRed, kClearGreen, kClearBlue, kClearAlpha);
        glClear(GL_COLOR_BUFFER_BIT);

        std::vector<int16_t> silence(kSamplesPerRenderFrame * kSilenceChannels, 0);
        std::vector<unsigned char> pixels;

        for (unsigned frame = 0; frame <= lastFrame && ok; ++frame)
        {
            if (feeder == nil || ![feeder feedNextFrameInto:context])
                [context pushPCMInt16:silence.data() frames:kSamplesPerRenderFrame channels:kSilenceChannels];

            [context renderFrameWithDeltaTime:kFrameSeconds targetFramebuffer:framebuffer.Id()];

            if (![wanted containsObject:@(frame)])
                continue;

            framebuffer.Bind();
            NSURL *pngURL = [self fileURLForFrame:frame extension:@"png"];

            if (!render_gl::ReadbackRgbTopDown(static_cast<int>(SVCaptureWidth), static_cast<int>(SVCaptureHeight), pixels)
                || !render_gl::WritePngRgbTopDown(pngURL.fileSystemRepresentation, static_cast<int>(SVCaptureWidth), static_cast<int>(SVCaptureHeight), pixels))
            {
                if (error != nullptr)
                    *error = SVMakeError(SVErrorIO, [NSString stringWithFormat:@"could not read back or write frame %u to %@", frame, pngURL.path]);
                ok = NO;
                break;
            }

            [written addObject:pngURL];

            if (!self.writesFrameState)
                continue;

            NSURL *jsonURL = [self fileURLForFrame:frame extension:@"json"];

            if (![context dumpFrameStateToURL:jsonURL error:error])
            {
                ok = NO;
                break;
            }

            [written addObject:jsonURL];
        }

        framebuffer.Unbind();
    }

    [context invalidate];
    self.writtenFileURLs = written;
    return ok;
}

@end
