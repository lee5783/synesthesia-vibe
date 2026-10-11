//
//  SVWavFeeder.mm
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#import "SVWavFeeder.h"
#import "SVContext+Internal.h"

#include "wav_reader.h"

#include <vector>

namespace
{
    constexpr NSUInteger kDefaultSamplesPerRenderFrame = 735;
}

@implementation SVWavFeeder
{
    synesthesia_vibe_demo::WavReader _reader;
    std::vector<int16_t> _pcm;
}

- (nullable instancetype)initWithURL:(NSURL *)url error:(NSError **)error
{
    self = [super init];

    if (self == nil)
        return nil;

    _URL = [url copy];
    _samplesPerRenderFrame = kDefaultSamplesPerRenderFrame;

    if (!url.isFileURL || !_reader.Load(url.fileSystemRepresentation) || _reader.FrameCount() == 0)
    {
        if (error != nullptr)
            *error = SVMakeError(SVErrorIO, [NSString stringWithFormat:@"could not load 16-bit PCM WAV %@", url.path]);
        return nil;
    }

    return self;
}

- (NSInteger)sampleRate
{
    return _reader.SampleRate();
}

- (NSInteger)channels
{
    return _reader.Channels();
}

- (NSUInteger)frameCount
{
    return _reader.FrameCount();
}

- (BOOL)feedNextFrameInto:(SVContext *)context
{
    if (_samplesPerRenderFrame == 0 || !_reader.FeedNextRenderFrame(_samplesPerRenderFrame, _pcm))
        return NO;

    [context pushPCMInt16:_pcm.data() frames:_samplesPerRenderFrame channels:_reader.Channels()];
    return YES;
}

@end
