//
//  wav_feeder_web.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

#include "wav_reader.h"

#include <emscripten/emscripten.h>

#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace
{
    struct WavFeederState
    {
        synesthesia_vibe_demo::WavReader reader;
        std::vector<int16_t> pcm;
    };
}

extern "C" EMSCRIPTEN_KEEPALIVE WavFeederState* sv_web_wav_open(const char* path)
{
    if (path == nullptr)
        return nullptr;

    WavFeederState* state = new (std::nothrow) WavFeederState();

    if (state == nullptr)
        return nullptr;

    if (!state->reader.Load(std::string(path)) || state->reader.FrameCount() == 0)
    {
        delete state;
        return nullptr;
    }

    return state;
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_wav_info(WavFeederState* state, int32_t* outInfo)
{
    if (state == nullptr || outInfo == nullptr)
        return;

    outInfo[0] = static_cast<int32_t>(state->reader.Channels());
    outInfo[1] = static_cast<int32_t>(state->reader.SampleRate());
    outInfo[2] = static_cast<int32_t>(state->reader.FrameCount());
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_wav_read(WavFeederState* state, int frames, int16_t* outInterleaved)
{
    if (state == nullptr || outInterleaved == nullptr || frames <= 0)
        return 0;

    const size_t frameCount = static_cast<size_t>(frames);

    if (frameCount > state->reader.FrameCount())
        return 0;

    if (!state->reader.FeedNextRenderFrame(frameCount, state->pcm))
        return 0;

    const size_t sampleCount = frameCount * static_cast<size_t>(state->reader.Channels());
    std::memcpy(outInterleaved, state->pcm.data(), sampleCount * sizeof(int16_t));
    return frames;
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_wav_close(WavFeederState* state)
{
    delete state;
}
