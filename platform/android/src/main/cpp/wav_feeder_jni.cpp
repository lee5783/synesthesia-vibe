//
//  wav_feeder_jni.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#include "wav_reader.h"

#include <jni.h>

#include <cstdint>
#include <new>
#include <string>
#include <vector>

namespace
{
    constexpr int kInfoFields = 3;

    struct WavFeederState
    {
        synesthesia_vibe_demo::WavReader reader;
        std::vector<int16_t> pcm;
    };

    WavFeederState* ToState(jlong handle)
    {
        return reinterpret_cast<WavFeederState*>(static_cast<intptr_t>(handle));
    }

    std::string ToPath(JNIEnv* env, jbyteArray bytes)
    {
        jsize length = env->GetArrayLength(bytes);
        std::string text(static_cast<size_t>(length), '\0');

        if (length > 0)
            env->GetByteArrayRegion(bytes, 0, length, reinterpret_cast<jbyte*>(&text[0]));

        return text;
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_WavFeeder_nativeOpen(JNIEnv* env, jclass, jbyteArray path)
{
    if (path == nullptr)
        return 0;

    std::string wavPath = ToPath(env, path);
    WavFeederState* state = new (std::nothrow) WavFeederState();

    if (state == nullptr)
        return 0;

    if (!state->reader.Load(wavPath) || state->reader.FrameCount() == 0)
    {
        delete state;
        return 0;
    }

    return static_cast<jlong>(reinterpret_cast<intptr_t>(state));
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_WavFeeder_nativeInfo(JNIEnv* env, jclass, jlong handle, jlongArray outInfo)
{
    WavFeederState* state = ToState(handle);

    if (state == nullptr || outInfo == nullptr || env->GetArrayLength(outInfo) < kInfoFields)
        return;

    jlong info[kInfoFields] = {
        static_cast<jlong>(state->reader.Channels()),
        static_cast<jlong>(state->reader.SampleRate()),
        static_cast<jlong>(state->reader.FrameCount()),
    };
    env->SetLongArrayRegion(outInfo, 0, kInfoFields, info);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_WavFeeder_nativeRead(JNIEnv* env, jclass, jlong handle, jint frames, jshortArray outInterleaved)
{
    WavFeederState* state = ToState(handle);

    if (state == nullptr || outInterleaved == nullptr || frames <= 0)
        return 0;

    const size_t frameCount = static_cast<size_t>(frames);

    if (frameCount > state->reader.FrameCount())
        return 0;

    const size_t sampleCount = frameCount * static_cast<size_t>(state->reader.Channels());

    if (static_cast<size_t>(env->GetArrayLength(outInterleaved)) < sampleCount)
        return 0;

    if (!state->reader.FeedNextRenderFrame(frameCount, state->pcm))
        return 0;

    env->SetShortArrayRegion(outInterleaved, 0, static_cast<jsize>(sampleCount), state->pcm.data());
    return frames;
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_WavFeeder_nativeClose(JNIEnv*, jclass, jlong handle)
{
    delete ToState(handle);
}
