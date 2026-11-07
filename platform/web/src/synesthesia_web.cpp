//
//  synesthesia_web.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-12.
//

#include <synesthesia-vibe/synesthesia-vibe.h>

#include "capture.h"
#include "framebuffer.h"
#include "gl_headers.h"
#include "wav_reader.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5_webgl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
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
    constexpr size_t kValidateErrorBytes = 1024;

    struct WebDebugReport
    {
        double time;
        uint32_t sequence;
        uint32_t frame;
        float displayFps;
        float simStepsPerFrame;
        float playbackRate;
        int32_t presetVersion;
        int32_t warpShader;
        int32_t compShader;
        float bass;
        float mid;
        float treb;
        float bassAtt;
        float midAtt;
        float trebAtt;
        int32_t texsizeX;
        int32_t texsizeY;
        int32_t width;
        int32_t height;
        int32_t feedbackFormat;
        int32_t glErrors;
        int32_t limiterActive;
    };

    static_assert(offsetof(WebDebugReport, time) == 0, "debug report layout is read by synesthesia-context.js");
    static_assert(offsetof(WebDebugReport, sequence) == 8, "debug report layout is read by synesthesia-context.js");
    static_assert(offsetof(WebDebugReport, limiterActive) == 88, "debug report layout is read by synesthesia-context.js");

    std::string g_glStrings;
    std::string g_captureError;
    std::string g_validateError;
    int g_prepareStatus = SV_OK;
    WebDebugReport g_debugReport = {};
    std::string g_debugPresetName;
    std::string g_debugLastError;

    void* GetGlProcAddress(const char* name)
    {
        return emscripten_webgl_get_proc_address(name);
    }

    std::string ToString(const char* text)
    {
        return text != nullptr ? std::string(text) : std::string();
    }

    void CopyDebugInfo(const sv_debug_info* info, void*)
    {
        WebDebugReport& report = g_debugReport;
        report.time = info->time;
        report.frame = info->frame;
        report.displayFps = info->display_fps;
        report.simStepsPerFrame = info->sim_steps_per_frame;
        report.playbackRate = info->playback_rate;
        report.presetVersion = info->preset_version;
        report.warpShader = info->warp_shader;
        report.compShader = info->comp_shader;
        report.bass = info->bass;
        report.mid = info->mid;
        report.treb = info->treb;
        report.bassAtt = info->bass_att;
        report.midAtt = info->mid_att;
        report.trebAtt = info->treb_att;
        report.texsizeX = info->texsize_x;
        report.texsizeY = info->texsize_y;
        report.width = info->width;
        report.height = info->height;
        report.feedbackFormat = info->feedback_format;
        report.glErrors = info->gl_errors;
        report.limiterActive = info->limiter_active;
        g_debugPresetName = ToString(info->preset_name);
        g_debugLastError = ToString(info->last_error);
        ++report.sequence;
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

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_init_gl()
{
    if (emscripten_webgl_get_current_context() == 0)
        return SV_ERR_GL;

    sv_status status = sv_init_gl(GetGlProcAddress);

    if (status == SV_OK)
    {
        std::string description = DescribeGl();
        printf("sv_init_gl OK\n%s", description.c_str());
    }

    return status;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* sv_web_gl_strings()
{
    g_glStrings = DescribeGl();
    return g_glStrings.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE sv_context* sv_web_create(
    int width, int height, int meshX, int meshY, int texsizeX, int texsizeY, float fpsHint,
    const char* resourcePath, const char* textureSearchPath, const char* cachePath, uint32_t rngSeed, int deterministic,
    int pinDate, const float* pinnedDate, int pinMouse, float mouseX, float mouseY,
    float maxFps, float maxLuminance, float flashLimit)
{
    std::string resource = ToString(resourcePath);
    std::string textureSearch = ToString(textureSearchPath);
    std::string cache = ToString(cachePath);
    float date[kPinnedDateFields] = {};

    if (pinnedDate != nullptr)
        std::memcpy(date, pinnedDate, sizeof(date));

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
    config.rng_seed = rngSeed;
    config.deterministic = deterministic != 0;
    config.pin_date = pinDate != 0;
    config.year = date[0];
    config.month = date[1];
    config.day = date[2];
    config.weekday = date[3];
    config.hour = date[4];
    config.minute = date[5];
    config.second = date[6];
    config.milliseconds = date[7];
    config.pin_mouse = pinMouse != 0;
    config.mouse_x = mouseX;
    config.mouse_y = mouseY;
    config.max_fps = maxFps;
    config.max_luminance = maxLuminance;
    config.flash_limit = flashLimit;

    g_debugReport = {};
    g_debugPresetName.clear();
    g_debugLastError.clear();
    return sv_create(&config);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_destroy(sv_context* ctx)
{
    sv_destroy(ctx);
    g_debugReport = {};
    g_debugPresetName.clear();
    g_debugLastError.clear();
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_load_preset(sv_context* ctx, const char* path, float blendSeconds)
{
    if (path == nullptr)
        return SV_ERR_ARG;

    return sv_load_preset(ctx, path, blendSeconds);
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_load_preset_text(sv_context* ctx, const char* text, size_t length, float blendSeconds)
{
    if (text == nullptr)
        return SV_ERR_ARG;

    return sv_load_preset_memory(ctx, text, length, blendSeconds);
}

extern "C" EMSCRIPTEN_KEEPALIVE sv_prepared_preset* sv_web_prepare_preset(sv_context* ctx, const char* path)
{
    char error[kValidateErrorBytes] = {};
    sv_prepared_preset* prepared = nullptr;
    sv_status status = path == nullptr ? SV_ERR_ARG : sv_prepare_preset(ctx, path, &prepared, error, sizeof(error));
    g_validateError = error;
    g_prepareStatus = status;
    return status == SV_OK ? prepared : nullptr;
}

extern "C" EMSCRIPTEN_KEEPALIVE sv_prepared_preset* sv_web_prepare_preset_text(sv_context* ctx, const char* text, size_t length)
{
    char error[kValidateErrorBytes] = {};
    sv_prepared_preset* prepared = nullptr;
    sv_status status = text == nullptr ? SV_ERR_ARG : sv_prepare_preset_memory(ctx, text, length, &prepared, error, sizeof(error));
    g_validateError = error;
    g_prepareStatus = status;
    return status == SV_OK ? prepared : nullptr;
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_prepare_status()
{
    return g_prepareStatus;
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_commit_preset(sv_context* ctx, sv_prepared_preset* prepared, float blendSeconds)
{
    return sv_commit_preset(ctx, prepared, blendSeconds);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_free_prepared(sv_prepared_preset* prepared)
{
    sv_prepared_preset_free(prepared);
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_validate_preset(const char* path)
{
    char error[kValidateErrorBytes] = {};
    sv_status status = sv_validate_preset(path, error, sizeof(error));
    g_validateError = error;
    return status;
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_validate_preset_text(const char* text, size_t length)
{
    char error[kValidateErrorBytes] = {};
    sv_status status = sv_validate_preset_memory(text, length, error, sizeof(error));
    g_validateError = error;
    return status;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* sv_web_validate_error()
{
    return g_validateError.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_push_pcm_float(sv_context* ctx, const float* interleaved, int frames, int channels)
{
    if (interleaved == nullptr || frames <= 0 || channels <= 0)
        return;

    sv_push_pcm_float(ctx, interleaved, static_cast<size_t>(frames), channels);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_push_pcm_s16(sv_context* ctx, const int16_t* interleaved, int frames, int channels)
{
    if (interleaved == nullptr || frames <= 0 || channels <= 0)
        return;

    sv_push_pcm_s16(ctx, interleaved, static_cast<size_t>(frames), channels);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_get_audio_levels(sv_context* ctx, float* outLevels)
{
    float levels[3] = {};
    sv_get_audio_levels(ctx, &levels[0], &levels[1], &levels[2]);

    if (outLevels != nullptr)
        std::memcpy(outLevels, levels, sizeof(levels));
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_render(sv_context* ctx, double deltaSeconds, uint32_t targetFramebuffer)
{
    sv_render_frame(ctx, deltaSeconds, targetFramebuffer);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_resize(sv_context* ctx, int width, int height)
{
    sv_resize(ctx, width, height);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_pause(sv_context* ctx)
{
    sv_pause(ctx);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_resume(sv_context* ctx)
{
    sv_resume(ctx);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_set_playback_rate(sv_context* ctx, float rate)
{
    sv_set_playback_rate(ctx, rate);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_set_debug_enabled(sv_context* ctx, int enabled)
{
    sv_set_debug_callback(ctx, enabled != 0 ? CopyDebugInfo : nullptr, nullptr);
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_set_debug_interval(sv_context* ctx, float seconds)
{
    sv_set_debug_interval(ctx, seconds);
}

extern "C" EMSCRIPTEN_KEEPALIVE const void* sv_web_debug_report()
{
    return &g_debugReport;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* sv_web_debug_preset_name()
{
    return g_debugPresetName.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* sv_web_debug_last_error()
{
    return g_debugLastError.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* sv_web_last_error(sv_context* ctx)
{
    const char* error = sv_last_error(ctx);
    return error != nullptr ? error : "";
}

extern "C" EMSCRIPTEN_KEEPALIVE void sv_web_dump_frame_state(sv_context* ctx, const char* jsonPath)
{
    if (jsonPath == nullptr)
        return;

    sv_dump_frame_state(ctx, jsonPath);
}

extern "C" EMSCRIPTEN_KEEPALIVE int sv_web_capture(sv_context* ctx, const char* wavPath, const int* frames, int frameCount,
    const char* outputDirectory, int writesFrameState)
{
    g_captureError.clear();
    sv_status status = SV_ERR_ARG;

    if (frames != nullptr && frameCount > 0 && outputDirectory != nullptr)
    {
        std::vector<int> wanted(frames, frames + frameCount);
        std::sort(wanted.begin(), wanted.end());
        wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
        wanted.erase(wanted.begin(), std::lower_bound(wanted.begin(), wanted.end(), 0));
        if (!wanted.empty())
            status = CaptureFrames(ctx, ToString(wavPath), wanted, ToString(outputDirectory), writesFrameState != 0, g_captureError);
    }

    if (status == SV_ERR_ARG && g_captureError.empty())
        g_captureError = "capture needs at least one non-negative frame and an output directory";

    return status;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* sv_web_capture_error()
{
    return g_captureError.c_str();
}
