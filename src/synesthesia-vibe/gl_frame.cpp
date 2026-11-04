//
//  gl_frame.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "gl_pipeline.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "blend_state.h"
#include "capture.h"
#include "context.h"
#include "custom_shapes.h"
#include "custom_waves.h"
#include "gl_error.h"
#include "motion_vectors.h"
#include "screen_borders.h"
#include "show_to_user.h"
#include "show_to_user_shaders.h"
#include "warp_blit.h"
#include "warp_blit_shaders.h"
#include "wave_draw.h"

namespace
{
    constexpr float kBeatdropRandStart[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    constexpr float kPixelCenterOffsetSignX = 0.9f;
    constexpr float kPixelCenterOffsetSignY = -0.9f;

    void BdGlSetPixelCenterOffset(BdGlPipeline* gl, bool enabled, int sizeX, int sizeY)
    {
        const float scale = enabled ? 1.0f : 0.0f;

        const float offsetX = scale * kPixelCenterOffsetSignX / static_cast<float>(sizeX);
        const float offsetY = scale * kPixelCenterOffsetSignY / static_cast<float>(sizeY);

        gl->untexturedProgram.Bind();
        gl->untexturedProgram.SetPositionOffset(offsetX, offsetY);

        gl->texturedProgram.Bind();
        gl->texturedProgram.SetPositionOffset(offsetX, offsetY);
    }

    void BdGlSetPixelCenterOffset(sv_context* ctx, BdGlPipeline* gl, bool enabled)
    {
        BdGlSetPixelCenterOffset(gl, enabled, ctx->texsizeX, ctx->texsizeY);
    }

    void BdGlScaleWarpTexelResidual(sv_context* ctx)
    {
        const float residualScale = 1.0f - ctx->stepFraction;
        const float du = residualScale * (0.5f - 0.5f * kPixelCenterOffsetSignX) / static_cast<float>(ctx->texsizeX);
        const float dv = residualScale * (0.5f + 0.5f * kPixelCenterOffsetSignY) / static_cast<float>(ctx->texsizeY);

        for (render_core::Vertex& vert : ctx->meshVerts)
        {
            vert.tu -= du;
            vert.tv -= dv;
        }
    }

    void BdGlStageMotionVectors(sv_context* ctx, BdGlPipeline* gl)
    {
        render_gl::MotionVectorsInput input;
        input.state = ctx->state;
        input.verts = ctx->meshVerts.data();
        input.gridX = ctx->nGridX;
        input.gridY = ctx->nGridY;
        input.texSizeX = ctx->texsizeX;
        input.stepFraction = ctx->stepFraction;

        gl->targets.Source().Bind();

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::DrawMotionVectors(input, gl->untexturedProgram, gl->quadMesh);
        BdGlSetPixelCenterOffset(ctx, gl, false);
    }

    void BdGlStageFirstFrameClear(sv_context* ctx, BdGlPipeline* gl)
    {
        if (ctx->framesSinceResize == 0)
            gl->targets.ClearSource();
    }

    FrameContext BdMakeShaderFrameContext(sv_context* ctx)
    {
        return FrameContext{
            ctx->mysound,
            static_cast<float>(ctx->timeModel.GetTime()),
            ctx->timeModel.GetFps(),
            ctx->frame,
            ctx->startTime,
            ctx->presetStartTime,
            ctx->nextPresetTime,
            ctx->width,
            ctx->height,
            kBeatdropScreenDependentRenderMode,
            ctx->mouseX,
            ctx->mouseY,
            ctx->mouseX,
            ctx->mouseY,
            ctx->mouseDown,
            ctx->mouseClicked,
            ctx->year,
            ctx->month,
            ctx->day,
            ctx->weekday,
            ctx->hour,
            ctx->minute,
            ctx->second,
            ctx->totalSeconds };
    }

    render_gl::PresetFrameInput BdMakePresetFrameInput(sv_context* ctx, BdGlPipeline* gl, const FrameContext* frameContext)
    {
        render_gl::PresetFrameInput frame;
        frame.ctx = frameContext;
        frame.state = ctx->state;
        frame.randFrame = render_core::ShaderVec4{ ctx->randFrame.x, ctx->randFrame.y, ctx->randFrame.z, ctx->randFrame.w };
        frame.texSizeX = ctx->texsizeX;
        frame.texSizeY = ctx->texsizeY;
        frame.mainTexture = 0;
        frame.highestBlurTexUsed = &gl->highestBlurTexUsed;
        return frame;
    }

    void BdGlStageWarp(sv_context* ctx, BdGlPipeline* gl)
    {
        render_core::GridAlphaInput input;
        input.state = ctx->state;
        input.oldState = ctx->oldState;
        input.time = static_cast<float>(ctx->timeModel.GetTime());
        input.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        input.aspectX = ctx->aspectX;
        input.aspectY = ctx->aspectY;
        input.invAspectX = ctx->invAspectX;
        input.invAspectY = ctx->invAspectY;
        input.texSizeX = ctx->texsizeX;
        input.texSizeY = ctx->texsizeY;
        input.gridX = ctx->nGridX;
        input.gridY = ctx->nGridY;
        input.stepFraction = ctx->stepFraction;

        render_core::ComputeGridAlphaValues(input, ctx->meshVerts.data(), ctx->meshVertInfo.data());

        if (ctx->stepFraction != 1.0f)
            BdGlScaleWarpTexelResidual(ctx);

        render_gl::WarpBlitInput blit;
        blit.verts = ctx->meshVerts.data();
        blit.vertexCount = static_cast<int>(ctx->meshVerts.size());
        blit.indexCount = gl->warpIndexCount;
        blit.decay = static_cast<float>(*ctx->state->var_pf_decay);
        blit.wrap = *ctx->state->var_pf_wrap > ctx->snapPoint;
        blit.texSizeX = ctx->texsizeX;
        blit.texSizeY = ctx->texsizeY;
        blit.stepFraction = ctx->stepFraction;

        if (gl->warpShaderActive)
        {
            const FrameContext frameContext = BdMakeShaderFrameContext(ctx);

            render_gl::WarpShaderBlitInput shaderBlit;
            shaderBlit.verts = ctx->meshVerts.data();
            shaderBlit.vertexCount = static_cast<int>(ctx->meshVerts.size());
            shaderBlit.gridX = ctx->nGridX;
            shaderBlit.gridY = ctx->nGridY;
            shaderBlit.indexCount = gl->warpIndexCount;
            shaderBlit.frame = BdMakePresetFrameInput(ctx, gl, &frameContext);

            render_gl::WarpedBlitShaders(shaderBlit, gl->warpInputs, gl->warpMesh, gl->warpVerts,
                gl->targets.Source(), gl->targets.Dest());
            return;
        }

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::WarpedBlitNoShaders(blit, gl->texturedProgram, gl->warpMesh, gl->warpVerts,
            gl->targets.Source(), gl->targets.Dest());
        BdGlSetPixelCenterOffset(ctx, gl, false);
    }

    void BdGlDumpBlurLevels(sv_context* ctx, BdGlPipeline* gl)
    {
        for (int i = 0; i < render_gl::kBlurTextureCount; i++)
        {
            gl->blur.Level(i).Bind();

            std::vector<unsigned char> pixels;
            if (!render_gl::ReadbackRgbTopDown(gl->blur.Width(i), gl->blur.Height(i), pixels))
                continue;

            char name[64];
            snprintf(name, sizeof(name), "/frame_%04d_blur%d.png", ctx->frame, i);
            render_gl::WritePngRgbTopDown((gl->blurDumpDir + name).c_str(), gl->blur.Width(i), gl->blur.Height(i), pixels);
        }
    }

    void BdGlStageBlur(sv_context* ctx, BdGlPipeline* gl)
    {
        const int levelsUsed = std::max(std::max(gl->highestBlurTexUsed, gl->forcedBlurLevels), gl->presentBlurTexUsed);
        gl->highestBlurTexUsed = 0;

        if (levelsUsed <= 0)
            return;

        render_gl::BlurChainInput input;
        input.edgeDarken = static_cast<float>(*ctx->state->var_pf_blur1_edge_darken);
        input.highestBlurTexUsed = levelsUsed;
        input.sourceWidth = ctx->width;
        input.sourceHeight = ctx->height;
        render_core::GetSafeBlurMinMax(ctx->state, input.blurMin, input.blurMax);

        gl->blur.Run(input, gl->targets.Source().ColorTexture());

        if (!gl->blurDumpDir.empty() && ctx->frame == gl->blurDumpFrame)
            BdGlDumpBlurLevels(ctx, gl);

        gl->targets.Dest().Bind();
        render_gl::SetBlendOpaque();
    }

    void BdGlStageCustomShapes(sv_context* ctx, BdGlPipeline* gl)
    {
        const FrameInputs frameInputs = BdMakeFrameInputs(ctx);

        render_gl::CustomShapesInput input;
        input.state = ctx->state;
        input.oldState = ctx->oldState;
        input.frameInputs = &frameInputs;
        input.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        input.aspectY = ctx->aspectY;
        input.texSizeX = ctx->texsizeX;
        input.texSizeY = ctx->texsizeY;
        input.stepFraction = ctx->stepFraction;

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::DrawCustomShapes(input, gl->texturedProgram, gl->untexturedProgram, gl->quadMesh,
            gl->targets.Source().ColorTexture());
        BdGlSetPixelCenterOffset(ctx, gl, false);
    }

    void BdGlStageCustomWaves(sv_context* ctx, BdGlPipeline* gl)
    {
        const FrameInputs frameInputs = BdMakeFrameInputs(ctx);

        render_gl::CustomWavesInput input;
        input.state = ctx->state;
        input.oldState = ctx->oldState;
        input.frameInputs = &frameInputs;
        input.sound = &ctx->sound;
        input.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        input.invAspectX = ctx->invAspectX;
        input.invAspectY = ctx->invAspectY;
        input.texSizeX = ctx->texsizeX;
        input.texSizeY = ctx->texsizeY;
        input.stepFraction = ctx->stepFraction;

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::DrawCustomWaves(input, gl->untexturedProgram, gl->quadMesh);
        BdGlSetPixelCenterOffset(ctx, gl, false);
    }

    void BdGlStageWave(sv_context* ctx, BdGlPipeline* gl)
    {
        render_gl::WaveDrawInput input;
        input.state = ctx->state;
        input.mysound = &ctx->mysound;
        input.time = static_cast<float>(ctx->timeModel.GetTime());
        input.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        input.aspectX = ctx->aspectX;
        input.aspectY = ctx->aspectY;
        input.texSizeX = ctx->texsizeX;
        input.texSizeY = ctx->texsizeY;
        input.stepFraction = ctx->stepFraction;

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::DrawWave(input, ctx->mysound.fWave[0], ctx->mysound.fWave[1], gl->untexturedProgram, gl->quadMesh);
        BdGlSetPixelCenterOffset(ctx, gl, false);
        render_gl::SetBlendOpaque();
    }

    void BdGlStageBorders(sv_context* ctx, BdGlPipeline* gl)
    {
        render_gl::BorderDrawInput input;
        input.state = ctx->state;
        input.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        input.aspectY = ctx->aspectY;
        input.stepFraction = ctx->stepFraction;

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::DrawDarkenCenter(input, gl->untexturedProgram, gl->quadMesh);
        render_gl::DrawBorders(input, gl->untexturedProgram, gl->quadMesh);
        BdGlSetPixelCenterOffset(ctx, gl, false);
    }

    void BdGlStageComposite(sv_context* ctx, BdGlPipeline* gl, uint32_t targetFbo, render_gl::Texture& compSource,
        render_gl::Texture& noShaderSource)
    {
        const float time = static_cast<float>(ctx->timeModel.GetTime());

        render_gl::ShowToUserInput show;
        show.width = ctx->width;
        show.height = ctx->height;
        show.invAspectY = ctx->invAspectY;
        show.screenDependentRenderMode = kBeatdropScreenDependentRenderMode;
        show.wrap = *ctx->state->var_pf_wrap > ctx->snapPoint;
        show.time = time;
        show.shaderAmount = ctx->state->m_fShader.eval(time);

        for (int i = 0; i < 4; i++)
            show.randStart[i] = kBeatdropRandStart[i];

        show.echoZoom = static_cast<float>(*ctx->state->var_pf_echo_zoom);
        show.echoAlpha = static_cast<float>(*ctx->state->var_pf_echo_alpha);
        show.echoOrient = static_cast<int>(*ctx->state->var_pf_echo_orient) % 4;
        show.gamma = static_cast<float>(*ctx->state->var_pf_gamma);
        show.brighten = *ctx->state->var_pf_brighten != 0.0;
        show.darken = *ctx->state->var_pf_darken != 0.0;
        show.solarize = *ctx->state->var_pf_solarize != 0.0;
        show.invert = *ctx->state->var_pf_invert != 0.0;

        render_gl::BindExternalTarget(targetFbo, ctx->width, ctx->height);

        if (gl->compShaderActive)
        {
            const FrameContext frameContext = BdMakeShaderFrameContext(ctx);

            render_gl::ShowToUserShaderInput shaderShow;
            shaderShow.width = ctx->width;
            shaderShow.height = ctx->height;
            shaderShow.time = time;

            for (int i = 0; i < 4; i++)
                shaderShow.randStart[i] = kBeatdropRandStart[i];

            shaderShow.frame = BdMakePresetFrameInput(ctx, gl, &frameContext);

            render_gl::ShowToUserShaders(shaderShow, gl->compInputs, gl->compGrid, gl->compMesh, compSource);
            return;
        }

        BdGlSetPixelCenterOffset(gl, true, ctx->width, ctx->height);
        render_gl::ShowToUserNoShaders(show, gl->texturedProgram, gl->untexturedProgram, gl->quadMesh, noShaderSource);
        BdGlSetPixelCenterOffset(gl, false, ctx->width, ctx->height);
    }

    render_gl::LuminanceLimiterInput BdGlLimiterInput(sv_context* ctx, float presentedTime)
    {
        const float dt = std::min(std::max(presentedTime - ctx->limiterLastTime, 0.0f), 0.25f);
        ctx->limiterLastTime = presentedTime;

        render_gl::LuminanceLimiterInput input;
        input.maxLuminance = ctx->config.max_luminance;
        input.flashLimit = ctx->config.flash_limit;
        input.dtSeconds = dt;
        return input;
    }

    void BdGlCompositeToTarget(sv_context* ctx, BdGlPipeline* gl, uint32_t targetFbo, render_gl::Texture& compSource,
        render_gl::Texture& noShaderSource)
    {
        if (!gl->limiterEnabled)
        {
            BdGlStageComposite(ctx, gl, targetFbo, compSource, noShaderSource);
            return;
        }

        const render_gl::LuminanceLimiterInput input = BdGlLimiterInput(ctx, static_cast<float>(ctx->timeModel.GetTime()));

        BdGlStageComposite(ctx, gl, gl->limiter.SceneFramebufferId(), compSource, noShaderSource);

        gl->limiter.Apply(input, targetFbo);
    }

    bool BdGlEnsureInterpolator(sv_context* ctx, BdGlPipeline* gl)
    {
        if (gl->interpolator.Valid())
            return true;

        if (gl->interpolatorFailed)
            return false;

        if (gl->interpolator.Create(ctx->width, ctx->height))
        {
            fprintf(stderr, "synesthesia-vibe: frame interpolator ready size=%dx%d\n", ctx->width, ctx->height);
            return true;
        }

        gl->interpolator.Destroy();
        gl->interpolatorFailed = true;
        fprintf(stderr, "synesthesia-vibe: frame interpolator unavailable, frames hold below 1x\n");
        return false;
    }

    void BdGlUploadAudio(BdGlPipeline* gl, sv_context* ctx)
    {
        const bool bAudioSampled = (gl->warpShaderActive && gl->warpInputs.UsesAudio()) ||
            (gl->compShaderActive && gl->compInputs.UsesAudio());

        if (bAudioSampled)
            gl->audio.Upload(ctx->fftSmoothed, ctx->fftPeak, ctx->sound.fWaveform[0], ctx->sound.fWaveform[1]);
    }

    void BdGlRunSimulationStages(sv_context* ctx, BdGlPipeline* gl)
    {
        BdGlStageMotionVectors(ctx, gl);
        BdGlStageFirstFrameClear(ctx, gl);
        BdGlStageWarp(ctx, gl);
        BdGlStageBlur(ctx, gl);
        BdGlStageCustomShapes(ctx, gl);
        BdGlStageCustomWaves(ctx, gl);
        BdGlStageWave(ctx, gl);
        BdGlStageBorders(ctx, gl);
    }

    BdGlPipeline* BdGlReadyPipeline(sv_context* ctx)
    {
        if (ctx == nullptr || ctx->gl == nullptr || !ctx->gl->ready)
            return nullptr;

        return ctx->gl;
    }
}

void BdGlRenderFrame(sv_context* ctx, uint32_t targetFbo)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    render_gl::SetDefaultDrawState();
    BdGlUploadAudio(gl, ctx);

    BdGlRunSimulationStages(ctx, gl);
    BdGlCompositeToTarget(ctx, gl, targetFbo, gl->targets.Source().ColorTexture(), gl->targets.Dest().ColorTexture());

    gl->targets.Swap();
    ctx->framesSinceResize++;

    gl->glErrorCount += render_gl::DrainGlErrors("BdGlRenderFrame");
}

void BdGlBeginFrame(sv_context* ctx)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    render_gl::SetDefaultDrawState();
    BdGlUploadAudio(gl, ctx);
}

void BdGlStepFrame(sv_context* ctx)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    render_gl::SetDefaultDrawState();
    BdGlRunSimulationStages(ctx, gl);

    gl->targets.Swap();
    ctx->framesSinceResize++;

    gl->glErrorCount += render_gl::DrainGlErrors("BdGlStepFrame");
}

void BdGlPresentFrame(sv_context* ctx, uint32_t targetFbo)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    render_gl::SetDefaultDrawState();
    BdGlCompositeToTarget(ctx, gl, targetFbo, gl->targets.Dest().ColorTexture(), gl->targets.Source().ColorTexture());
    gl->presentBlurTexUsed = gl->highestBlurTexUsed;

    gl->glErrorCount += render_gl::DrainGlErrors("BdGlPresentFrame");
}

void BdGlPresentInterpolatedFrame(sv_context* ctx, uint32_t targetFbo, bool stepped, bool restart, float alpha)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    if (!BdGlEnsureInterpolator(ctx, gl))
    {
        BdGlPresentFrame(ctx, targetFbo);
        return;
    }

    render_gl::SetDefaultDrawState();

    if (restart)
        gl->interpolator.ResetHistory();

    if (stepped || !gl->interpolator.HasHistory())
    {
        BdGlStageComposite(ctx, gl, gl->interpolator.CaptureFramebufferId(), gl->targets.Dest().ColorTexture(),
            gl->targets.Source().ColorTexture());
        gl->interpolator.CommitCapture();
        gl->presentBlurTexUsed = gl->highestBlurTexUsed;
    }

    if (!gl->limiterEnabled)
    {
        gl->interpolator.Blend(alpha, targetFbo);
    }
    else
    {
        const float time = static_cast<float>(ctx->timeModel.GetTime());
        const float lag = gl->interpolator.HasPrevious() ? (1.0f - alpha) * static_cast<float>(kBeatdropSimulationStepSeconds) : 0.0f;
        const render_gl::LuminanceLimiterInput input = BdGlLimiterInput(ctx, time - lag);

        gl->interpolator.Blend(alpha, gl->limiter.SceneFramebufferId());
        gl->limiter.Apply(input, targetFbo);
    }

    gl->glErrorCount += render_gl::DrainGlErrors("BdGlPresentInterpolatedFrame");
}
