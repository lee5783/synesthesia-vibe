//
//  context.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "context.h"
#include "gl_pipeline.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "parser_win32.h"
#include "rng_compat.h"

namespace
{
    void BdBuildWarpIndices(sv_context* ctx);

    void BdSetDefaultAnalysisSettings(AudioAnalysisSettings& settings)
    {
        settings.bassStart = 0;
        settings.bassEnd = 250;
        settings.midStart = 250;
        settings.midEnd = 4000;
        settings.trebStart = 4000;
        settings.trebEnd = 20000;
        settings.fftShaderScaling = 0.175f;
        settings.fftShaderNoiseFloor = 0.03f;
        settings.audioSensitivity = 1.0f;
    }

    void BdBuildMeshGrid(sv_context* ctx)
    {
        const int nVertsX = ctx->nGridX + 1;
        const int nVertsY = ctx->nGridY + 1;

        ctx->meshVerts.assign(static_cast<size_t>(nVertsX) * static_cast<size_t>(nVertsY), render_core::Vertex());
        ctx->meshVertInfo.assign(static_cast<size_t>(nVertsX) * static_cast<size_t>(nVertsY), render_core::VertInfo());

        const float texelOffsetX = 0.5f / static_cast<float>(ctx->texsizeX);
        const float texelOffsetY = 0.5f / static_cast<float>(ctx->texsizeY);

        int nVert = 0;
        for (int y = 0; y <= ctx->nGridY; y++)
        {
            for (int x = 0; x <= ctx->nGridX; x++)
            {
                render_core::Vertex& vert = ctx->meshVerts[nVert];
                render_core::VertInfo& info = ctx->meshVertInfo[nVert];

                vert.x = x / static_cast<float>(ctx->nGridX) * 2.0f - 1.0f;
                vert.y = y / static_cast<float>(ctx->nGridY) * 2.0f - 1.0f;
                vert.z = 0.0f;

                if (kBeatdropScreenDependentRenderMode)
                    info.rad = sqrtf(vert.x * vert.x + vert.y * vert.y);
                else
                    info.rad = sqrtf(vert.x * vert.x * ctx->aspectX * ctx->aspectX + vert.y * vert.y * ctx->aspectY * ctx->aspectY);
                if (y == ctx->nGridY / 2 && x == ctx->nGridX / 2)
                    info.ang = 0.0f;
                else if (kBeatdropScreenDependentRenderMode)
                    info.ang = atan2f(vert.y, vert.x);
                else
                    info.ang = atan2f(vert.y * ctx->aspectY, vert.x * ctx->aspectX);
                info.a = 1.0f;
                info.c = 0.0f;

                vert.rad = info.rad;
                vert.ang = info.ang;
                vert.tu_orig = vert.x * 0.5f + 0.5f + texelOffsetX;
                vert.tv_orig = -vert.y * 0.5f + 0.5f + texelOffsetY;
                vert.tu = vert.tu_orig;
                vert.tv = vert.tv_orig;

                nVert++;
            }
        }

        BdBuildWarpIndices(ctx);
    }

    void BdBuildWarpIndices(sv_context* ctx)
    {
        ctx->meshIndices.clear();
        ctx->meshIndices.reserve(static_cast<size_t>(ctx->nGridX) * static_cast<size_t>(ctx->nGridY) * 6);

        for (int quadrant = 0; quadrant < 4; quadrant++)
        {
            for (int slice = 0; slice < ctx->nGridY / 2; slice++)
            {
                for (int i = 0; i < ctx->nGridX / 2; i++)
                {
                    int xref = i;
                    int yref = slice;

                    if (quadrant & 1)
                        xref = ctx->nGridX - 1 - xref;
                    if (quadrant & 2)
                        yref = ctx->nGridY - 1 - yref;

                    const int v = xref + yref * (ctx->nGridX + 1);

                    ctx->meshIndices.push_back(v);
                    ctx->meshIndices.push_back(v + 1);
                    ctx->meshIndices.push_back(v + ctx->nGridX + 1);
                    ctx->meshIndices.push_back(v + 1);
                    ctx->meshIndices.push_back(v + ctx->nGridX + 1);
                    ctx->meshIndices.push_back(v + ctx->nGridX + 2);
                }
            }
        }
    }

    constexpr int kTexsizeBlock = 16;

    enum BdTexsizeMode
    {
        kTexsizeFixed = 0,
        kTexsizeExact = 1,
        kTexsizeSnapped = 2
    };

    int BdSnapToBlock(int size)
    {
        return (size + kTexsizeBlock - 1) / kTexsizeBlock * kTexsizeBlock;
    }

    int BdClassifyTexsize(int texsize, int size)
    {
        if (texsize == BdSnapToBlock(size))
            return kTexsizeSnapped;
        if (texsize == size)
            return kTexsizeExact;
        return kTexsizeFixed;
    }

    int BdDeriveTexsize(int mode, int size, int fixedSize)
    {
        if (mode == kTexsizeSnapped)
            return BdSnapToBlock(size);
        if (mode == kTexsizeExact)
            return size;
        return fixedSize;
    }

    void BdUpdateAspect(sv_context* ctx)
    {
        render_core::ComputeAspectRatios(ctx->texsizeX, ctx->texsizeY, &ctx->aspectX, &ctx->aspectY, &ctx->invAspectX, &ctx->invAspectY);
    }
}

std::wstring BdUtf8ToWide(const char* utf8)
{
    std::wstring out;
    if (utf8 == nullptr)
        return out;

    const unsigned char* p = reinterpret_cast<const unsigned char*>(utf8);
    while (*p != 0)
    {
        unsigned long cp = *p;
        int extra = 0;
        if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
        else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
        else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }

        for (int i = 0; i < extra; i++)
        {
            p++;
            if (*p == 0)
                break;
            cp = (cp << 6) | (*p & 0x3F);
        }
        p++;

        out.push_back(static_cast<wchar_t>(cp));
    }

    return out;
}

std::string BdWideToUtf8(const wchar_t* wide)
{
    std::string out;
    if (wide == nullptr)
        return out;

    for (const wchar_t* p = wide; *p != 0; p++)
    {
        unsigned long cp = static_cast<unsigned long>(static_cast<unsigned int>(*p));
        if (cp < 0x80)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    return out;
}

bool BdFileExists(const wchar_t* path)
{
    if (path == nullptr || path[0] == 0)
        return false;

    FILE* f = ParserWfopen(path, L"rb");
    if (f == nullptr)
        return false;

    fclose(f);
    return true;
}

void BdCopyString(char* dst, int dstBytes, const char* src)
{
    if (dst == nullptr || dstBytes <= 0)
        return;

    if (src == nullptr)
    {
        dst[0] = 0;
        return;
    }

    int i = 0;
    for (; i < dstBytes - 1 && src[i] != 0; i++)
        dst[i] = src[i];
    dst[i] = 0;
}

static void BdConsumeDx9StartupDefaults(sv_context* ctx)
{
    constexpr int kDx9ExtraOldStateDefaultCalls = 1;

    ctx->state->Default();
    ctx->oldState->Default();

    for (int i = 0; i < kDx9ExtraOldStateDefaultCalls; i++)
        ctx->oldState->Default();
}

extern "C" sv_context* sv_create(const sv_config* config)
{
    sv_context* ctx = new sv_context();

    if (config != nullptr)
        ctx->config = *config;
    else
        memset(&ctx->config, 0, sizeof(ctx->config));

    if (ctx->config.width <= 0)
        ctx->config.width = 640;
    if (ctx->config.height <= 0)
        ctx->config.height = 480;
    if (ctx->config.mesh_x <= 0)
        ctx->config.mesh_x = kBeatdropDefaultGridX;
    if (ctx->config.mesh_y <= 0)
        ctx->config.mesh_y = kBeatdropDefaultGridY;
    if (ctx->config.texsize_x <= 0)
        ctx->config.texsize_x = ctx->config.width;
    if (ctx->config.texsize_y <= 0)
        ctx->config.texsize_y = ctx->config.height;
    if (ctx->config.fps_hint <= 0.0f)
        ctx->config.fps_hint = kBeatdropDefaultFps;

    ctx->width = ctx->config.width;
    ctx->height = ctx->config.height;
    ctx->texsizeX = ctx->config.texsize_x;
    ctx->texsizeY = ctx->config.texsize_y;
    ctx->texsizeModeX = BdClassifyTexsize(ctx->texsizeX, ctx->width);
    ctx->texsizeModeY = BdClassifyTexsize(ctx->texsizeY, ctx->height);
    ctx->meshX = ctx->config.mesh_x;
    ctx->meshY = ctx->config.mesh_y;
    ctx->fpsHint = ctx->config.fps_hint;

    ctx->deterministic = ctx->config.deterministic;
    ctx->pinDate = ctx->config.pin_date;
    ctx->pinMouse = ctx->config.pin_mouse;
    ctx->paused = false;

    if (ctx->config.resource_path != nullptr)
        ctx->resourcePath = ctx->config.resource_path;
    if (ctx->config.texture_search_path != nullptr)
        ctx->textureSearchPath = ctx->config.texture_search_path;
    if (ctx->config.cache_path != nullptr)
        ctx->cachePath = ctx->config.cache_path;

    ctx->snapPoint = 0.5f;
    ctx->frame = 0;
    ctx->startTime = 0.0f;
    ctx->presetStartTime = -1.0f;
    ctx->nextPresetTime = -1.0f;
    ctx->nextPresetTimeWasInitialized = false;
    ctx->lastBlendSeconds = 0.0f;
    ctx->lastFrameDt = 0.0;
    ctx->lastTargetFbo = 0;

    ctx->nGridX = ctx->meshX;
    ctx->nGridY = ctx->meshY;

    BdUpdateAspect(ctx);
    BdBuildMeshGrid(ctx);

    ctx->gl = nullptr;
    ctx->framesSinceResize = 0;

    memset(&ctx->sound, 0, sizeof(ctx->sound));
    memset(&ctx->mysound, 0, sizeof(ctx->mysound));
    memset(ctx->fftSmoothed, 0, sizeof(ctx->fftSmoothed));
    memset(ctx->fftPeak, 0, sizeof(ctx->fftPeak));
    memset(ctx->nFftPeakHold, 0, sizeof(ctx->nFftPeakHold));
    memset(ctx->frozenBeatDetection, 0, sizeof(ctx->frozenBeatDetection));
    ctx->freezeBeatDetection = false;

    memset(ctx->oldWave[0], 0, sizeof(float) * kBeatdropWaveformSamples);
    memset(ctx->oldWave[1], 0, sizeof(float) * kBeatdropWaveformSamples);
    ctx->oldWaveInitialized = true;

    memset(ctx->shaderIncludeText, 0, sizeof(ctx->shaderIncludeText));
    memset(ctx->defaultWarpShaderText, 0, sizeof(ctx->defaultWarpShaderText));
    memset(ctx->defaultCompShaderText, 0, sizeof(ctx->defaultCompShaderText));

    memset(ctx->pcmLeft, 0, sizeof(ctx->pcmLeft));
    memset(ctx->pcmRight, 0, sizeof(ctx->pcmRight));

    BdSetDefaultAnalysisSettings(ctx->analysisSettings);
    g_fAudioSensitivity = ctx->analysisSettings.audioSensitivity;

    ctx->fft.Init(MY_FFT_WINDOW, MY_FFT_SAMPLES, -1);
    ctx->fftShader.Init(MY_FFT_SHADER_WINDOW, MY_FFT_SHADER_SAMPLES, 0);

    ctx->timeModel.SetHighPrecisionFrequency(static_cast<long long>(kBeatdropTicksPerSecond));
    ctx->rawClock.SetDeterministic(ctx->deterministic);
    ctx->rawClock.SetTickDelta(166666);
    ctx->rawClock.Restart();

    ctx->hostSink.SetTimeSource(&ctx->timeModel);

    ctx->rng.seed(ctx->config.rng_seed);
    if (ctx->deterministic)
        BdSrand(ctx->config.rng_seed);
    ctx->randFrame = Vec4f(0.0f, 0.0f, 0.0f, 0.0f);

    ctx->state = new CState();
    ctx->oldState = new CState();
    BdConsumeDx9StartupDefaults(ctx);

    BdGlPipelineCreate(ctx);

    return ctx;
}

extern "C" void sv_destroy(sv_context* ctx)
{
    if (ctx == nullptr)
        return;

    BdGlPipelineDestroy(ctx);

    ctx->state->FreeVarsAndCode();
    ctx->oldState->FreeVarsAndCode();

    delete ctx->state;
    delete ctx->oldState;

    ctx->fft.CleanUp();
    ctx->fftShader.CleanUp();

    delete ctx;
}

extern "C" void sv_resize(sv_context* ctx, int width, int height)
{
    if (ctx == nullptr || width <= 0 || height <= 0)
        return;

    ctx->width = width;
    ctx->height = height;

    ctx->texsizeX = BdDeriveTexsize(ctx->texsizeModeX, width, ctx->texsizeX);
    ctx->texsizeY = BdDeriveTexsize(ctx->texsizeModeY, height, ctx->texsizeY);

    BdUpdateAspect(ctx);
    BdBuildMeshGrid(ctx);
    BdGlPipelineResize(ctx);
}

extern "C" void sv_pause(sv_context* ctx)
{
    if (ctx == nullptr)
        return;

    ctx->paused = true;
}

extern "C" void sv_resume(sv_context* ctx)
{
    if (ctx == nullptr || !ctx->paused)
        return;

    ctx->paused = false;

    if (!ctx->deterministic)
        ctx->rawClock.Restart();
}

extern "C" const char* sv_last_error(sv_context* ctx)
{
    if (ctx == nullptr)
        return "";

    return ctx->lastError.c_str();
}