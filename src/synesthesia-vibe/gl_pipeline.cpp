//
//  gl_pipeline.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "gl_pipeline.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <utility>

#include "context.h"
#include "gl_error.h"
#include "gl_loader.h"
#include "gl_object_counts.h"

namespace
{
    class BdZeroRawClock : public render_core::IRawClock
    {
    public:
        bool QueryHighPrecisionTicks(long long& outTicks) override
        {
            outTicks = 0;
            return true;
        }

        unsigned QueryLowPrecisionMilliseconds() override
        {
            return 0;
        }
    };

    bool BdGlBuildNoise(sv_context* ctx)
    {
        BdGlPipeline* gl = ctx->gl;

        std::random_device randomDevice;
        gl->pickRng.seed(ctx->deterministic ? static_cast<uint64_t>(ctx->config.rng_seed) : static_cast<uint64_t>(randomDevice()));
        std::mt19937_64& gen = gl->pickRng;

        render_gl::NoiseVerifyOptions verify;
        verify.szDumpDir = getenv("SYNESTHESIA_VIBE_NOISE_DUMP_DIR");
        verify.enabled = getenv("SYNESTHESIA_VIBE_VERIFY_NOISE") != nullptr || (verify.szDumpDir != nullptr && verify.szDumpDir[0] != 0);

        if (ctx->deterministic)
        {
            BdZeroRawClock zeroClock;
            return gl->noise.Create(gen, zeroClock, verify);
        }

        return gl->noise.Create(gen, ctx->rawClock, verify);
    }

    void BdGlReadDevOverrides(BdGlPipeline* gl)
    {
        const char* forceBlur = getenv("SYNESTHESIA_VIBE_FORCE_BLUR_LEVELS");
        gl->forcedBlurLevels = forceBlur != nullptr ? atoi(forceBlur) : 0;

        const char* blurDumpDir = getenv("SYNESTHESIA_VIBE_BLUR_DUMP_DIR");
        gl->blurDumpDir = blurDumpDir != nullptr ? blurDumpDir : "";

        const char* blurDumpFrame = getenv("SYNESTHESIA_VIBE_BLUR_DUMP_FRAME");
        gl->blurDumpFrame = blurDumpFrame != nullptr ? atoi(blurDumpFrame) : 0;
    }

    bool BdGlUploadWarpIndices(sv_context* ctx)
    {
        BdGlPipeline* gl = ctx->gl;

        gl->warpIndexCount = static_cast<int>(ctx->meshIndices.size());
        return gl->warpMesh.UploadIndices(ctx->meshIndices.data(), gl->warpIndexCount);
    }

    bool BdGlBuildCompositeGrid(sv_context* ctx)
    {
        return ctx->gl->compGrid.Build(ctx->gl->compMesh, ctx->width, ctx->height, kBeatdropScreenDependentRenderMode,
            ctx->aspectX, ctx->aspectY);
    }

    bool BdGlBuildResources(sv_context* ctx)
    {
        BdGlPipeline* gl = ctx->gl;

        if (!gl->targets.Create(ctx->texsizeX, ctx->texsizeY))
            return false;

        if (!gl->texturedProgram.Build(true) || !gl->untexturedProgram.Build(false))
            return false;

        if (!gl->warpMesh.Create() || !gl->quadMesh.Create() || !gl->compMesh.Create())
            return false;

        if (!BdGlBuildNoise(ctx))
            return false;

        if (!gl->blur.Create(ctx->texsizeX, ctx->texsizeY))
            return false;

        if (!gl->audio.Create())
            return false;

        const std::string texturesDir = ctx->resourcePath + "/textures";
        gl->disk.SetDirectories(texturesDir, std::string(), ctx->textureSearchPath);
        gl->picker.Configure(texturesDir, &gl->pickRng);

        gl->warpVerts.assign(ctx->meshVerts.size(), render_core::Vertex());

        if (gl->limiterEnabled && !gl->limiter.Create(ctx->width, ctx->height))
            return false;

        return BdGlUploadWarpIndices(ctx) && BdGlBuildCompositeGrid(ctx);
    }

    std::string BdGlBindStage(sv_context* ctx, render_gl::PresetShaderStage stage, const char* stageName, const char* shaderText,
        const BdPreparedStage* prepared, render_gl::PresetShaderInputs& inputs, bool& active, std::string& fallback)
    {
        BdGlPipeline* gl = ctx->gl;
        active = false;

        const render_gl::PresetProgram* program = prepared != nullptr && prepared->translated && prepared->hlsl == shaderText
            ? gl->presetCache.GetOrBuildTranslated(stage, shaderText, prepared->shader)
            : gl->presetCache.GetOrBuild(stage, shaderText);

        render_gl::PresetShaderResources resources = {};
        resources.noise = &gl->noise;
        resources.blur = &gl->blur;
        resources.audio = &gl->audio;
        resources.disk = &gl->disk;
        resources.picker = &gl->picker;

        if (program->shader != nullptr && inputs.Resolve(*program, resources))
        {
            active = true;

            std::string warnings;

            for (const std::string& warning : inputs.Warnings())
            {
                if (!warnings.empty())
                    warnings += "; ";

                warnings += warning;
            }

            if (warnings.empty())
                return std::string();

            return std::string(stageName) + " shader input warnings: " + warnings;
        }

        std::string message(stageName);
        message += " shader unavailable, using fixed-function path: ";
        message += program->error.empty() ? "program has no linked shader" : program->error;
        fallback = message;
        return message;
    }
}

extern "C" sv_status sv_init_gl(sv_gl_get_proc_fn get_proc)
{
    if (get_proc == nullptr)
        return SV_ERR_ARG;

    return render_gl::LoadGlFunctions(get_proc) ? SV_OK : SV_ERR_GL;
}

void BdGlPipelineCreate(sv_context* ctx)
{
    if (ctx == nullptr || ctx->gl != nullptr)
        return;

    if (!render_gl::GlFunctionsLoaded())
    {
        ctx->lastError = "renderer unavailable: call sv_init_gl on a current GL context before sv_create";
        return;
    }

    render_gl::DrainGlErrors("BdGlPipelineCreate entry");

    ctx->gl = new BdGlPipeline();
    ctx->gl->highestBlurTexUsed = 0;
    ctx->gl->presentBlurTexUsed = 0;
    ctx->gl->forcedBlurLevels = 0;
    ctx->gl->blurDumpFrame = 0;
    ctx->gl->warpIndexCount = 0;
    ctx->gl->glErrorCount = 0;
    ctx->gl->warpShaderActive = false;
    ctx->gl->compShaderActive = false;
    ctx->gl->oldWarpShaderActive = false;
    ctx->gl->oldCompShaderActive = false;
    ctx->gl->warpInputs = &ctx->gl->warpInputSlots[0];
    ctx->gl->compInputs = &ctx->gl->compInputSlots[0];
    ctx->gl->oldWarpInputs = &ctx->gl->warpInputSlots[1];
    ctx->gl->oldCompInputs = &ctx->gl->compInputSlots[1];
    ctx->gl->resourcesBuilt = false;
    ctx->gl->ready = false;
    ctx->gl->limiterEnabled = ctx->config.max_luminance > 0.0f || ctx->config.flash_limit > 0.0f;
    ctx->gl->presetCache.SetTranslationCache(&ctx->translationCache);

    BdGlReadDevOverrides(ctx->gl);

    if (!BdGlBuildResources(ctx))
    {
        fprintf(stderr, "synesthesia-vibe: GL pipeline create failed at texsize %dx%d\n", ctx->texsizeX, ctx->texsizeY);
        ctx->lastError = "GL pipeline create failed";
        return;
    }

    ctx->gl->resourcesBuilt = true;
    ctx->gl->ready = true;
    ctx->framesSinceResize = 0;

    fprintf(stderr, "synesthesia-vibe: GL pipeline ready texsize=%dx%d grid=%dx%d\n", ctx->texsizeX, ctx->texsizeY, ctx->nGridX, ctx->nGridY);

    if (ctx->gl->limiterEnabled)
        fprintf(stderr, "synesthesia-vibe: luminance limiter on max_luminance=%.3f flash_limit=%.3f\n", ctx->config.max_luminance,
            ctx->config.flash_limit);
}

void BdGlPipelineResize(sv_context* ctx)
{
    if (ctx == nullptr || ctx->gl == nullptr || !ctx->gl->resourcesBuilt)
        return;

    BdGlPipeline* gl = ctx->gl;

    if (!gl->targets.Resize(ctx->texsizeX, ctx->texsizeY))
    {
        fprintf(stderr, "synesthesia-vibe: GL target resize failed at texsize %dx%d\n", ctx->texsizeX, ctx->texsizeY);
        ctx->lastError = "GL target resize failed";
        gl->ready = false;
        return;
    }

    if (!gl->blur.Resize(ctx->texsizeX, ctx->texsizeY))
    {
        fprintf(stderr, "synesthesia-vibe: GL blur resize failed at texsize %dx%d\n", ctx->texsizeX, ctx->texsizeY);
        ctx->lastError = "GL blur resize failed";
        gl->ready = false;
        return;
    }

    gl->warpVerts.assign(ctx->meshVerts.size(), render_core::Vertex());

    if (gl->limiterEnabled && !gl->limiter.Resize(ctx->width, ctx->height))
    {
        ctx->lastError = "GL limiter resize failed";
        gl->ready = false;
        return;
    }

    if (!BdGlUploadWarpIndices(ctx) || !BdGlBuildCompositeGrid(ctx))
    {
        ctx->lastError = "GL mesh rebuild failed";
        gl->ready = false;
        return;
    }

    gl->ready = true;

    ctx->framesSinceResize = 0;

    fprintf(stderr, "synesthesia-vibe: GL pipeline resized size=%dx%d texsize=%dx%d\n", ctx->width, ctx->height, ctx->texsizeX, ctx->texsizeY);
}

void BdGlPipelineDestroy(sv_context* ctx)
{
    if (ctx == nullptr || ctx->gl == nullptr)
        return;

    BdGlPipeline* gl = ctx->gl;
    const int errorCount = gl->glErrorCount;

    delete gl;
    ctx->gl = nullptr;

    fprintf(stderr, "synesthesia-vibe: GL pipeline destroyed gl_errors=%d\n", errorCount);
    render_gl::LogGlObjectCounts("sv_destroy");
}

std::string BdGlPreparePresetShaders(sv_context* ctx, const std::string& presetDir,
    const BdPreparedStage* preparedWarp, const BdPreparedStage* preparedComp, std::string* fallbackReason)
{
    if (fallbackReason != nullptr)
        fallbackReason->clear();

    if (ctx == nullptr)
        return std::string();

    if (ctx->gl == nullptr || !ctx->gl->ready)
    {
        if (ctx->gl != nullptr)
        {
            ctx->gl->warpShaderActive = false;
            ctx->gl->compShaderActive = false;
            ctx->gl->oldWarpShaderActive = false;
            ctx->gl->oldCompShaderActive = false;
        }

        if (fallbackReason != nullptr)
            *fallbackReason = ctx->gl == nullptr ? "no GL pipeline" : "GL pipeline not ready";

        return std::string();
    }

    BdGlPipeline* gl = ctx->gl;

    std::swap(gl->warpInputs, gl->oldWarpInputs);
    std::swap(gl->compInputs, gl->oldCompInputs);
    gl->oldWarpShaderActive = gl->warpShaderActive;
    gl->oldCompShaderActive = gl->compShaderActive;

    gl->presetCache.BeginGeneration();
    gl->disk.BeginGeneration();
    gl->disk.SetDirectories(ctx->resourcePath + "/textures", presetDir, ctx->textureSearchPath);
    gl->picker.Rescan();

    std::string errors;
    std::string warpFallback;
    std::string compFallback;

    if (ctx->state->m_nWarpPSVersion > 0)
        errors += BdGlBindStage(ctx, render_gl::PresetShaderStage::Warp, "warp", ctx->state->m_szWarpShadersText,
            preparedWarp, *gl->warpInputs, gl->warpShaderActive, warpFallback);
    else
        gl->warpShaderActive = false;

    if (ctx->state->m_nCompPSVersion > 0)
    {
        const std::string compErrors = BdGlBindStage(ctx, render_gl::PresetShaderStage::Composite, "comp",
            ctx->state->m_szCompShadersText, preparedComp, *gl->compInputs, gl->compShaderActive, compFallback);

        if (!errors.empty() && !compErrors.empty())
            errors += "; ";

        errors += compErrors;
    }
    else
    {
        gl->compShaderActive = false;
    }

    gl->presetCache.TrimUnused();
    gl->disk.TrimUnused();

    if (fallbackReason != nullptr)
    {
        *fallbackReason = warpFallback;

        if (!warpFallback.empty() && !compFallback.empty())
            *fallbackReason += "; ";

        *fallbackReason += compFallback;
    }

    if (!errors.empty())
        fprintf(stderr, "synesthesia-vibe: %s\n", errors.c_str());

    return errors;
}
