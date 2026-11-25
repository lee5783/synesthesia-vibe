//
//  audio_router.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#include "audio_router.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace
{
    std::mutex g_routerMutex;
    std::vector<sv_context*> g_liveContexts;
    bool g_haveLevels = false;
    sv_context* g_levelsFrom = nullptr;
    float g_levels[3] = { 0.0f, 0.0f, 0.0f };
}

namespace svw
{

void RegisterAudioContext(sv_context* ctx)
{
    if (ctx == nullptr)
        return;

    std::lock_guard<std::mutex> lock(g_routerMutex);
    if (std::find(g_liveContexts.begin(), g_liveContexts.end(), ctx) == g_liveContexts.end())
        g_liveContexts.push_back(ctx);
}

void UnregisterAudioContext(sv_context* ctx)
{
    std::lock_guard<std::mutex> lock(g_routerMutex);
    g_liveContexts.erase(std::remove(g_liveContexts.begin(), g_liveContexts.end(), ctx), g_liveContexts.end());
    if (g_liveContexts.empty() || g_levelsFrom == ctx)
    {
        g_haveLevels = false;
        g_levelsFrom = nullptr;
    }
}

bool PushPcmToLiveContext(const float* interleaved, size_t frames, int channels)
{
    std::lock_guard<std::mutex> lock(g_routerMutex);
    if (g_liveContexts.empty())
        return false;

    sv_push_pcm_float(g_liveContexts.front(), interleaved, frames, channels);
    return true;
}

void PublishAudioLevels(sv_context* ctx)
{
    float levels[3] = { 0.0f, 0.0f, 0.0f };
    sv_get_audio_levels(ctx, &levels[0], &levels[1], &levels[2]);

    std::lock_guard<std::mutex> lock(g_routerMutex);
    std::copy(levels, levels + 3, g_levels);
    g_haveLevels = true;
    g_levelsFrom = ctx;
}

void WithdrawAudioLevels(sv_context* ctx)
{
    std::lock_guard<std::mutex> lock(g_routerMutex);
    if (ctx == nullptr || g_levelsFrom != ctx)
        return;

    g_haveLevels = false;
    g_levelsFrom = nullptr;
}

bool LatestAudioLevels(float& bass, float& mid, float& treb)
{
    std::lock_guard<std::mutex> lock(g_routerMutex);
    bass = g_haveLevels ? g_levels[0] : 0.0f;
    mid = g_haveLevels ? g_levels[1] : 0.0f;
    treb = g_haveLevels ? g_levels[2] : 0.0f;
    return g_haveLevels;
}

}
