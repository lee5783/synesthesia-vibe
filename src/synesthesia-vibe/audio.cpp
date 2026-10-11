//
//  audio.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include <cstdint>

#include "context.h"

extern "C" void sv_push_pcm_float(sv_context* ctx, const float* interleaved, size_t frames, int channels)
{
    if (ctx == nullptr || interleaved == nullptr || frames == 0 || channels <= 0)
        return;

    AudioCaptureFormat format;
    format.sampleRate = TARGET_SAMPLE_RATE;
    format.channels = channels;
    format.bitsPerSample = 32;
    format.blockAlign = channels * (format.bitsPerSample / 8);

    SetAudioBuf(reinterpret_cast<const unsigned char*>(interleaved), static_cast<uint32_t>(frames), format, false);
}

extern "C" void sv_push_pcm_s16(sv_context* ctx, const int16_t* interleaved, size_t frames, int channels)
{
    if (ctx == nullptr || interleaved == nullptr || frames == 0 || channels <= 0)
        return;

    AudioCaptureFormat format;
    format.sampleRate = TARGET_SAMPLE_RATE;
    format.channels = channels;
    format.bitsPerSample = 16;
    format.blockAlign = channels * (format.bitsPerSample / 8);

    SetAudioBuf(reinterpret_cast<const unsigned char*>(interleaved), static_cast<uint32_t>(frames), format, true);
}

extern "C" void sv_get_audio_levels(sv_context* ctx, float* out_bass, float* out_mid, float* out_treb)
{
    if (ctx == nullptr)
        return;

    if (out_bass != nullptr)
        *out_bass = ctx->mysound.imm_rel[0];

    if (out_mid != nullptr)
        *out_mid = ctx->mysound.imm_rel[1];

    if (out_treb != nullptr)
        *out_treb = ctx->mysound.imm_rel[2];
}
