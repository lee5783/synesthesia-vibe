//
//  synesthesia_jni.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#include <synesthesia-vibe/synesthesia-vibe.h>

#include "capture.h"
#include "framebuffer.h"
#include "gl_headers.h"
#include "wav_reader.h"

#include <EGL/egl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr const char* kLogTag = "SynesthesiaVibe";
    constexpr const char* kGlesLibrary = "libGLESv3.so";
    constexpr int kPinnedDateFields = 8;
    constexpr int kCaptureWidth = 838;
    constexpr int kCaptureHeight = 441;
    constexpr double kCaptureFrameSeconds = 1.0 / 60.0;
    constexpr size_t kSamplesPerRenderFrame = 735;
    constexpr int kSilenceChannels = 2;
    constexpr float kClearRed = 51.0f / 255.0f;
    constexpr float kClearGreen = 102.0f / 255.0f;
    constexpr float kClearBlue = 153.0f / 255.0f;
    constexpr float kClearAlpha = 1.0f;
    constexpr size_t kLogReadChunk = 512;
    constexpr size_t kValidateErrorBytes = 1024;
    constexpr int kDebugValueCount = 23;
    constexpr int kDebugStringCount = 3;
    constexpr int kPresetInfoValueCount = 5;

    void ForwardStderrToLogcat(int readFd)
    {
        std::string pending;
        char chunk[kLogReadChunk];

        for (;;)
        {
            ssize_t count = read(readFd, chunk, sizeof(chunk));

            if (count <= 0)
                break;

            pending.append(chunk, static_cast<size_t>(count));
            size_t newline;

            while ((newline = pending.find('\n')) != std::string::npos)
            {
                pending[newline] = '\0';
                __android_log_write(ANDROID_LOG_INFO, kLogTag, pending.c_str());
                pending.erase(0, newline + 1);
            }
        }

        if (!pending.empty())
            __android_log_write(ANDROID_LOG_INFO, kLogTag, pending.c_str());

        close(readFd);
    }

    void RedirectStderrToLogcatOnce()
    {
        static std::once_flag once;

        std::call_once(once, []
        {
            int fds[2];

            if (pipe(fds) != 0)
                return;

            if (dup2(fds[1], STDERR_FILENO) < 0)
            {
                close(fds[0]);
                close(fds[1]);
                return;
            }

            close(fds[1]);
            setvbuf(stderr, nullptr, _IOLBF, 0);
            std::thread(ForwardStderrToLogcat, fds[0]).detach();
        });
    }

    void* GetGlProcAddress(const char* name)
    {
        void* symbol = reinterpret_cast<void*>(eglGetProcAddress(name));

        if (symbol == nullptr)
        {
            static void* library = dlopen(kGlesLibrary, RTLD_NOW | RTLD_LOCAL);
            symbol = library != nullptr ? dlsym(library, name) : nullptr;
        }

        return symbol;
    }

    struct DebugSink
    {
        std::mutex mutex;
        jlong sequence = 0;
        double values[kDebugValueCount] = {};
        std::string presetName;
        std::string lastError;
        std::string fallbackReason;
    };

    std::mutex g_debugSinksMutex;
    std::unordered_map<jlong, std::unique_ptr<DebugSink>> g_debugSinks;

    DebugSink* FindDebugSink(jlong handle)
    {
        std::lock_guard<std::mutex> guard(g_debugSinksMutex);
        auto found = g_debugSinks.find(handle);
        return found != g_debugSinks.end() ? found->second.get() : nullptr;
    }

    DebugSink* AcquireDebugSink(jlong handle)
    {
        std::lock_guard<std::mutex> guard(g_debugSinksMutex);
        std::unique_ptr<DebugSink>& slot = g_debugSinks[handle];

        if (!slot)
            slot.reset(new DebugSink());

        return slot.get();
    }

    void ReleaseDebugSink(jlong handle)
    {
        std::lock_guard<std::mutex> guard(g_debugSinksMutex);
        g_debugSinks.erase(handle);
    }

    struct RequestSink
    {
        std::mutex mutex;
        jlong sequence = 0;
        float reason = 0.0f;
        float blendSeconds = 0.0f;
    };

    std::mutex g_requestSinksMutex;
    std::unordered_map<jlong, std::unique_ptr<RequestSink>> g_requestSinks;

    RequestSink* FindRequestSink(jlong handle)
    {
        std::lock_guard<std::mutex> guard(g_requestSinksMutex);
        auto found = g_requestSinks.find(handle);
        return found != g_requestSinks.end() ? found->second.get() : nullptr;
    }

    RequestSink* AcquireRequestSink(jlong handle)
    {
        std::lock_guard<std::mutex> guard(g_requestSinksMutex);
        std::unique_ptr<RequestSink>& slot = g_requestSinks[handle];

        if (!slot)
            slot.reset(new RequestSink());

        return slot.get();
    }

    void ReleaseRequestSink(jlong handle)
    {
        std::lock_guard<std::mutex> guard(g_requestSinksMutex);
        g_requestSinks.erase(handle);
    }

    void StorePresetRequest(sv_context*, const sv_preset_request* request, void* user)
    {
        RequestSink* sink = static_cast<RequestSink*>(user);

        if (request == nullptr || sink == nullptr)
            return;

        std::lock_guard<std::mutex> guard(sink->mutex);
        sink->reason = static_cast<float>(request->reason);
        sink->blendSeconds = request->suggested_blend_seconds;
        ++sink->sequence;
    }

    void StoreDebugInfo(const sv_debug_info* info, void* user)
    {
        DebugSink* sink = static_cast<DebugSink*>(user);

        if (info == nullptr || sink == nullptr)
            return;

        std::lock_guard<std::mutex> guard(sink->mutex);
        double* v = sink->values;
        v[0] = info->time;
        v[1] = info->frame;
        v[2] = info->display_fps;
        v[3] = info->sim_steps_per_frame;
        v[4] = info->playback_rate;
        v[5] = info->preset_version;
        v[6] = info->warp_shader;
        v[7] = info->comp_shader;
        v[8] = info->bass;
        v[9] = info->mid;
        v[10] = info->treb;
        v[11] = info->bass_att;
        v[12] = info->mid_att;
        v[13] = info->treb_att;
        v[14] = info->texsize_x;
        v[15] = info->texsize_y;
        v[16] = info->width;
        v[17] = info->height;
        v[18] = info->feedback_format;
        v[19] = info->gl_errors;
        v[20] = info->limiter_active;
        v[21] = info->warp_ps_version;
        v[22] = info->comp_ps_version;
        sink->presetName = info->preset_name != nullptr ? info->preset_name : "";
        sink->lastError = info->last_error != nullptr ? info->last_error : "";
        sink->fallbackReason = info->fallback_reason != nullptr ? info->fallback_reason : "";
        ++sink->sequence;
    }

    sv_context* ToContext(jlong handle)
    {
        return reinterpret_cast<sv_context*>(static_cast<intptr_t>(handle));
    }

    std::string ToString(JNIEnv* env, jbyteArray bytes)
    {
        if (bytes == nullptr)
            return std::string();

        jsize length = env->GetArrayLength(bytes);
        std::string text(static_cast<size_t>(length), '\0');

        if (length > 0)
            env->GetByteArrayRegion(bytes, 0, length, reinterpret_cast<jbyte*>(&text[0]));

        return text;
    }

    jbyteArray ToBytes(JNIEnv* env, const std::string& text)
    {
        jbyteArray bytes = env->NewByteArray(static_cast<jsize>(text.size()));

        if (bytes != nullptr && !text.empty())
            env->SetByteArrayRegion(bytes, 0, static_cast<jsize>(text.size()), reinterpret_cast<const jbyte*>(text.data()));

        return bytes;
    }

    const char* GlString(GLenum name)
    {
        const GLubyte* value = glGetString(name);
        return value != nullptr ? reinterpret_cast<const char*>(value) : "(null)";
    }

    std::string DescribeGl()
    {
        std::string text;

        if (glad_glGetString == nullptr)
            return text;

        text.append("GL_VENDOR=").append(GlString(GL_VENDOR)).append("\n");
        text.append("GL_RENDERER=").append(GlString(GL_RENDERER)).append("\n");
        text.append("GL_VERSION=").append(GlString(GL_VERSION)).append("\n");
        text.append("GL_SHADING_LANGUAGE_VERSION=").append(GlString(GL_SHADING_LANGUAGE_VERSION)).append("\n");
        text.append("AUDIO_TEXTURE_FORMAT=").append(GLAD_GL_OES_texture_float_linear ? "R32F" : "R16F").append("\n");
        return text;
    }

    std::string FramePath(const std::string& directory, int frame, const char* extension)
    {
        char name[32];
        snprintf(name, sizeof(name), "frame_%04d.%s", frame, extension);
        return directory + "/" + name;
    }

    sv_status CaptureFrames(sv_context* ctx, const std::string& wavPath, const std::vector<int>& frames, const std::string& outputDirectory, bool writesFrameState, std::string& error)
    {
        synesthesia_vibe_demo::WavReader reader;
        bool hasWav = !wavPath.empty();

        if (hasWav && (!reader.Load(wavPath) || reader.FrameCount() == 0))
        {
            error = "could not load 16-bit PCM WAV " + wavPath;
            return SV_ERR_IO;
        }

        render_gl::Framebuffer framebuffer;

        if (!framebuffer.Create(kCaptureWidth, kCaptureHeight))
        {
            error = "capture framebuffer could not be created";
            return SV_ERR_GL;
        }

        framebuffer.Bind();
        glClearColor(kClearRed, kClearGreen, kClearBlue, kClearAlpha);
        glClear(GL_COLOR_BUFFER_BIT);

        std::vector<int16_t> silence(kSamplesPerRenderFrame * kSilenceChannels, 0);
        std::vector<int16_t> pcm;
        std::vector<unsigned char> pixels;
        size_t nextWanted = 0;
        const int lastFrame = frames.back();
        sv_status status = SV_OK;

        for (int frame = 0; frame <= lastFrame; ++frame)
        {
            if (hasWav && reader.FeedNextRenderFrame(kSamplesPerRenderFrame, pcm))
                sv_push_pcm_s16(ctx, pcm.data(), kSamplesPerRenderFrame, reader.Channels());
            else
                sv_push_pcm_s16(ctx, silence.data(), kSamplesPerRenderFrame, kSilenceChannels);

            sv_render_frame(ctx, kCaptureFrameSeconds, framebuffer.Id());

            if (frames[nextWanted] != frame)
                continue;

            ++nextWanted;
            framebuffer.Bind();
            std::string pngPath = FramePath(outputDirectory, frame, "png");

            if (!render_gl::ReadbackRgbTopDown(kCaptureWidth, kCaptureHeight, pixels)
                || !render_gl::WritePngRgbTopDown(pngPath.c_str(), kCaptureWidth, kCaptureHeight, pixels))
            {
                error = "could not read back or write frame " + std::to_string(frame) + " to " + pngPath;
                status = SV_ERR_IO;
                break;
            }

            if (writesFrameState)
                sv_dump_frame_state(ctx, FramePath(outputDirectory, frame, "json").c_str());
        }

        framebuffer.Unbind();
        return status;
    }
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*)
{
    RedirectStderrToLogcatOnce();
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeInitGl(JNIEnv*, jclass)
{
    sv_status status = sv_init_gl(GetGlProcAddress);

    if (status == SV_OK)
    {
        std::string description = DescribeGl();
        __android_log_print(ANDROID_LOG_INFO, kLogTag, "sv_init_gl OK\n%s", description.c_str());
    }

    return status;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeGlStrings(JNIEnv* env, jclass)
{
    return ToBytes(env, DescribeGl());
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeCreate(JNIEnv* env, jclass,
    jint width, jint height, jint meshX, jint meshY, jint texsizeX, jint texsizeY, jfloat fpsHint,
    jbyteArray resourcePath, jbyteArray textureSearchPath, jbyteArray cachePath, jint rngSeed, jboolean deterministic,
    jboolean pinDate, jfloatArray pinnedDate, jboolean pinMouse, jfloat mouseX, jfloat mouseY,
    jfloat maxFps, jfloat maxLuminance, jfloat flashLimit)
{
    std::string resource = ToString(env, resourcePath);
    std::string textureSearch = ToString(env, textureSearchPath);
    std::string cache = ToString(env, cachePath);
    float date[kPinnedDateFields] = {};

    if (pinnedDate != nullptr && env->GetArrayLength(pinnedDate) >= kPinnedDateFields)
        env->GetFloatArrayRegion(pinnedDate, 0, kPinnedDateFields, date);

    sv_config config = {};
    config.width = width;
    config.height = height;
    config.mesh_x = meshX;
    config.mesh_y = meshY;
    config.texsize_x = texsizeX;
    config.texsize_y = texsizeY;
    config.fps_hint = fpsHint;
    config.resource_path = resourcePath != nullptr ? resource.c_str() : nullptr;
    config.texture_search_path = textureSearchPath != nullptr ? textureSearch.c_str() : nullptr;
    config.cache_path = cachePath != nullptr ? cache.c_str() : nullptr;
    config.rng_seed = static_cast<uint32_t>(rngSeed);
    config.deterministic = deterministic == JNI_TRUE;
    config.pin_date = pinDate == JNI_TRUE;
    config.year = date[0];
    config.month = date[1];
    config.day = date[2];
    config.weekday = date[3];
    config.hour = date[4];
    config.minute = date[5];
    config.second = date[6];
    config.milliseconds = date[7];
    config.pin_mouse = pinMouse == JNI_TRUE;
    config.mouse_x = mouseX;
    config.mouse_y = mouseY;
    config.max_fps = maxFps;
    config.max_luminance = maxLuminance;
    config.flash_limit = flashLimit;

    return static_cast<jlong>(reinterpret_cast<intptr_t>(sv_create(&config)));
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeDestroy(JNIEnv*, jclass, jlong handle)
{
    sv_destroy(ToContext(handle));
    ReleaseDebugSink(handle);
    ReleaseRequestSink(handle);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeLoadPreset(JNIEnv* env, jclass, jlong handle, jbyteArray path, jfloat blendSeconds)
{
    if (path == nullptr)
        return SV_ERR_ARG;

    std::string presetPath = ToString(env, path);
    return sv_load_preset(ToContext(handle), presetPath.c_str(), blendSeconds);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeLoadPresetText(JNIEnv* env, jclass, jlong handle, jbyteArray text, jfloat blendSeconds)
{
    if (text == nullptr)
        return SV_ERR_ARG;

    std::string presetText = ToString(env, text);
    return sv_load_preset_memory(ToContext(handle), presetText.data(), presetText.size(), blendSeconds);
}

namespace
{
    jlong ReportPrepare(JNIEnv* env, sv_status status, sv_prepared_preset* prepared, const char* error, jobjectArray errorOut, jintArray statusOut)
    {
        if (statusOut != nullptr && env->GetArrayLength(statusOut) > 0)
        {
            jint value = status;
            env->SetIntArrayRegion(statusOut, 0, 1, &value);
        }

        if (status != SV_OK && errorOut != nullptr && env->GetArrayLength(errorOut) > 0)
        {
            jbyteArray message = ToBytes(env, std::string(error));
            env->SetObjectArrayElement(errorOut, 0, message);
            env->DeleteLocalRef(message);
        }

        return status == SV_OK ? static_cast<jlong>(reinterpret_cast<intptr_t>(prepared)) : 0;
    }
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativePreparePreset(JNIEnv* env, jclass, jlong handle, jbyteArray path, jobjectArray errorOut, jintArray statusOut)
{
    if (path == nullptr)
        return ReportPrepare(env, SV_ERR_ARG, nullptr, "sv_prepare_preset: path is empty", errorOut, statusOut);

    std::string presetPath = ToString(env, path);
    char error[kValidateErrorBytes] = {};
    sv_prepared_preset* prepared = nullptr;
    sv_status status = sv_prepare_preset(ToContext(handle), presetPath.c_str(), &prepared, error, sizeof(error));
    return ReportPrepare(env, status, prepared, error, errorOut, statusOut);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativePreparePresetText(JNIEnv* env, jclass, jlong handle, jbyteArray text, jobjectArray errorOut, jintArray statusOut)
{
    if (text == nullptr)
        return ReportPrepare(env, SV_ERR_ARG, nullptr, "sv_prepare_preset_memory: text is null", errorOut, statusOut);

    std::string presetText = ToString(env, text);
    char error[kValidateErrorBytes] = {};
    sv_prepared_preset* prepared = nullptr;
    sv_status status = sv_prepare_preset_memory(ToContext(handle), presetText.data(), presetText.size(), &prepared, error, sizeof(error));
    return ReportPrepare(env, status, prepared, error, errorOut, statusOut);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeCommitPreset(JNIEnv*, jclass, jlong handle, jlong prepared, jfloat blendSeconds)
{
    return sv_commit_preset(ToContext(handle), reinterpret_cast<sv_prepared_preset*>(static_cast<intptr_t>(prepared)), blendSeconds);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeFreePrepared(JNIEnv*, jclass, jlong prepared)
{
    sv_prepared_preset_free(reinterpret_cast<sv_prepared_preset*>(static_cast<intptr_t>(prepared)));
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeValidatePreset(JNIEnv* env, jclass, jbyteArray path, jobjectArray errorOut)
{
    std::string presetPath = ToString(env, path);
    char error[kValidateErrorBytes] = {};
    sv_status status = sv_validate_preset(path != nullptr ? presetPath.c_str() : nullptr, error, sizeof(error));

    if (status != SV_OK && errorOut != nullptr && env->GetArrayLength(errorOut) > 0)
    {
        jbyteArray message = ToBytes(env, std::string(error));
        env->SetObjectArrayElement(errorOut, 0, message);
        env->DeleteLocalRef(message);
    }

    return status;
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativePushPcmFloat(JNIEnv* env, jclass, jlong handle, jfloatArray interleaved, jint frames, jint channels)
{
    if (interleaved == nullptr || frames <= 0 || channels <= 0)
        return;

    void* samples = env->GetPrimitiveArrayCritical(interleaved, nullptr);

    if (samples == nullptr)
        return;

    sv_push_pcm_float(ToContext(handle), static_cast<const float*>(samples), static_cast<size_t>(frames), channels);
    env->ReleasePrimitiveArrayCritical(interleaved, samples, JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativePushPcmShort(JNIEnv* env, jclass, jlong handle, jshortArray interleaved, jint frames, jint channels)
{
    if (interleaved == nullptr || frames <= 0 || channels <= 0)
        return;

    void* samples = env->GetPrimitiveArrayCritical(interleaved, nullptr);

    if (samples == nullptr)
        return;

    sv_push_pcm_s16(ToContext(handle), static_cast<const int16_t*>(samples), static_cast<size_t>(frames), channels);
    env->ReleasePrimitiveArrayCritical(interleaved, samples, JNI_ABORT);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeGetAudioLevels(JNIEnv* env, jclass, jlong handle, jfloatArray outLevels)
{
    float levels[3] = {};
    sv_get_audio_levels(ToContext(handle), &levels[0], &levels[1], &levels[2]);

    if (outLevels != nullptr && env->GetArrayLength(outLevels) >= 3)
        env->SetFloatArrayRegion(outLevels, 0, 3, levels);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeRender(JNIEnv*, jclass, jlong handle, jdouble deltaSeconds, jint targetFramebuffer)
{
    sv_render_frame(ToContext(handle), deltaSeconds, static_cast<uint32_t>(targetFramebuffer));
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeResize(JNIEnv*, jclass, jlong handle, jint width, jint height)
{
    sv_resize(ToContext(handle), width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativePause(JNIEnv*, jclass, jlong handle)
{
    sv_pause(ToContext(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeResume(JNIEnv*, jclass, jlong handle)
{
    sv_resume(ToContext(handle));
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeSetPlaybackRate(JNIEnv*, jclass, jlong handle, jfloat rate)
{
    sv_set_playback_rate(ToContext(handle), rate);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeSetTransitionConfig(JNIEnv*, jclass, jlong handle, jfloat softCutSeconds, jfloat presetDuration, jfloat presetDurationRand, jboolean hardCutsEnabled, jfloat hardCutLoudness, jfloat hardCutHalflife)
{
    sv_transition_config config = {};
    config.soft_cut_seconds = softCutSeconds;
    config.preset_duration = presetDuration;
    config.preset_duration_rand = presetDurationRand;
    config.hard_cuts_enabled = hardCutsEnabled == JNI_TRUE;
    config.hard_cut_loudness = hardCutLoudness;
    config.hard_cut_halflife = hardCutHalflife;
    sv_set_transition_config(ToContext(handle), &config);
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeSetBlendPattern(JNIEnv*, jclass, jlong handle, jint pattern)
{
    sv_set_blend_pattern(ToContext(handle), pattern);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeSetPresetRequestEnabled(JNIEnv*, jclass, jlong handle, jboolean enabled)
{
    if (enabled != JNI_TRUE)
    {
        sv_set_preset_request_callback(ToContext(handle), nullptr, nullptr);
        RequestSink* sink = FindRequestSink(handle);

        if (sink == nullptr)
            return 0;

        std::lock_guard<std::mutex> guard(sink->mutex);
        return sink->sequence;
    }

    RequestSink* sink = AcquireRequestSink(handle);
    sv_set_preset_request_callback(ToContext(handle), StorePresetRequest, sink);
    std::lock_guard<std::mutex> guard(sink->mutex);
    return sink->sequence;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativePresetRequestSequence(JNIEnv*, jclass, jlong handle)
{
    RequestSink* sink = FindRequestSink(handle);

    if (sink == nullptr)
        return 0;

    std::lock_guard<std::mutex> guard(sink->mutex);
    return sink->sequence;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeReadPresetRequest(JNIEnv* env, jclass, jlong handle, jfloatArray valuesOut)
{
    RequestSink* sink = FindRequestSink(handle);

    if (sink == nullptr || valuesOut == nullptr || env->GetArrayLength(valuesOut) < 2)
        return 0;

    float values[2];
    jlong sequence;

    {
        std::lock_guard<std::mutex> guard(sink->mutex);
        values[0] = sink->reason;
        values[1] = sink->blendSeconds;
        sequence = sink->sequence;
    }

    env->SetFloatArrayRegion(valuesOut, 0, 2, values);
    return sequence;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeSetDebugEnabled(JNIEnv*, jclass, jlong handle, jboolean enabled)
{
    if (enabled != JNI_TRUE)
    {
        sv_set_debug_callback(ToContext(handle), nullptr, nullptr);
        DebugSink* sink = FindDebugSink(handle);

        if (sink == nullptr)
            return 0;

        std::lock_guard<std::mutex> guard(sink->mutex);
        return sink->sequence;
    }

    DebugSink* sink = AcquireDebugSink(handle);
    sv_set_debug_callback(ToContext(handle), StoreDebugInfo, sink);
    std::lock_guard<std::mutex> guard(sink->mutex);
    return sink->sequence;
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeSetDebugInterval(JNIEnv*, jclass, jlong handle, jfloat seconds)
{
    sv_set_debug_interval(ToContext(handle), seconds);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeDebugSequence(JNIEnv*, jclass, jlong handle)
{
    DebugSink* sink = FindDebugSink(handle);

    if (sink == nullptr)
        return 0;

    std::lock_guard<std::mutex> guard(sink->mutex);
    return sink->sequence;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeReadDebugInfo(JNIEnv* env, jclass, jlong handle, jdoubleArray valuesOut, jobjectArray stringsOut)
{
    DebugSink* sink = FindDebugSink(handle);

    if (sink == nullptr || valuesOut == nullptr || stringsOut == nullptr)
        return 0;

    if (env->GetArrayLength(valuesOut) < kDebugValueCount || env->GetArrayLength(stringsOut) < kDebugStringCount)
        return 0;

    double values[kDebugValueCount];
    std::string presetName;
    std::string lastError;
    std::string fallbackReason;
    jlong sequence;

    {
        std::lock_guard<std::mutex> guard(sink->mutex);
        std::copy(sink->values, sink->values + kDebugValueCount, values);
        presetName = sink->presetName;
        lastError = sink->lastError;
        fallbackReason = sink->fallbackReason;
        sequence = sink->sequence;
    }

    env->SetDoubleArrayRegion(valuesOut, 0, kDebugValueCount, values);

    jbyteArray name = ToBytes(env, presetName);
    env->SetObjectArrayElement(stringsOut, 0, name);
    env->DeleteLocalRef(name);

    jbyteArray error = ToBytes(env, lastError);
    env->SetObjectArrayElement(stringsOut, 1, error);
    env->DeleteLocalRef(error);

    jbyteArray reason = ToBytes(env, fallbackReason);
    env->SetObjectArrayElement(stringsOut, 2, reason);
    env->DeleteLocalRef(reason);

    return sequence;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeGetPresetInfo(JNIEnv* env, jclass, jlong handle, jintArray valuesOut, jobjectArray reasonOut)
{
    if (valuesOut == nullptr || reasonOut == nullptr)
        return SV_ERR_ARG;

    if (env->GetArrayLength(valuesOut) < kPresetInfoValueCount || env->GetArrayLength(reasonOut) < 1)
        return SV_ERR_ARG;

    sv_preset_info info = {};
    sv_status status = sv_get_preset_info(ToContext(handle), &info);

    if (status != SV_OK)
        return status;

    jint values[kPresetInfoValueCount] = {info.preset_version, info.warp_ps_version, info.comp_ps_version, info.warp_shader, info.comp_shader};
    env->SetIntArrayRegion(valuesOut, 0, kPresetInfoValueCount, values);

    jbyteArray reason = ToBytes(env, std::string(info.fallback_reason));
    env->SetObjectArrayElement(reasonOut, 0, reason);
    env->DeleteLocalRef(reason);

    return status;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeLastError(JNIEnv* env, jclass, jlong handle)
{
    const char* error = sv_last_error(ToContext(handle));
    return ToBytes(env, error != nullptr ? std::string(error) : std::string());
}

extern "C" JNIEXPORT void JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeDumpFrameState(JNIEnv* env, jclass, jlong handle, jbyteArray jsonPath)
{
    if (jsonPath == nullptr)
        return;

    std::string path = ToString(env, jsonPath);
    sv_dump_frame_state(ToContext(handle), path.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_synesthesiavibe_SynesthesiaContext_nativeCapture(JNIEnv* env, jclass, jlong handle, jbyteArray wavPath, jintArray frames,
    jbyteArray outputDirectory, jboolean writesFrameState, jobjectArray errorOut)
{
    std::string error;
    sv_status status = SV_ERR_ARG;
    jsize frameCount = frames != nullptr ? env->GetArrayLength(frames) : 0;

    if (frameCount > 0 && outputDirectory != nullptr)
    {
        std::vector<int> wanted(static_cast<size_t>(frameCount));
        env->GetIntArrayRegion(frames, 0, frameCount, wanted.data());
        std::sort(wanted.begin(), wanted.end());
        wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
        wanted.erase(wanted.begin(), std::lower_bound(wanted.begin(), wanted.end(), 0));
        if (!wanted.empty())
            status = CaptureFrames(ToContext(handle), ToString(env, wavPath), wanted, ToString(env, outputDirectory), writesFrameState == JNI_TRUE, error);
    }

    if (status == SV_ERR_ARG && error.empty())
        error = "capture needs at least one non-negative frame and an output directory";

    if (status != SV_OK && errorOut != nullptr && env->GetArrayLength(errorOut) > 0)
    {
        jbyteArray message = ToBytes(env, error);
        env->SetObjectArrayElement(errorOut, 0, message);
        env->DeleteLocalRef(message);
    }

    return status;
}
