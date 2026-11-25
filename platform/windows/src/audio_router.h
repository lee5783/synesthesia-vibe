//
//  audio_router.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_AUDIO_ROUTER_H__
#define __SYNESTHESIA_VIBE_WINDOWS_AUDIO_ROUTER_H__ 1

#include <synesthesia-vibe/synesthesia-vibe.h>

#include <cstddef>

namespace svw
{

void RegisterAudioContext(sv_context* ctx);
void UnregisterAudioContext(sv_context* ctx);
bool PushPcmToLiveContext(const float* interleaved, size_t frames, int channels);
void PublishAudioLevels(sv_context* ctx);
void WithdrawAudioLevels(sv_context* ctx);
bool LatestAudioLevels(float& bass, float& mid, float& treb);

}

#endif
