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
#include "math_macros.h"
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
    constexpr float kMotionReleaseSeconds = 5.0f;

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

    void BdGlStageMotionVectors(sv_context* ctx, BdGlPipeline* gl)
    {
        render_gl::MotionVectorsInput input;
        input.state = ctx->state;
        input.verts = ctx->meshVerts.data();
        input.gridX = ctx->nGridX;
        input.gridY = ctx->nGridY;
        input.texSizeX = ctx->texsizeX;

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

    void BdGlWarpShaderPass(sv_context* ctx, BdGlPipeline* gl, bool oldPreset, bool alphaBlend)
    {
        const FrameContext frameContext = BdMakeShaderFrameContext(ctx);

        render_gl::WarpShaderBlitInput shaderBlit;
        shaderBlit.verts = ctx->meshVerts.data();
        shaderBlit.vertexCount = static_cast<int>(ctx->meshVerts.size());
        shaderBlit.gridX = ctx->nGridX;
        shaderBlit.gridY = ctx->nGridY;
        shaderBlit.indexCount = gl->warpIndexCount;
        shaderBlit.alphaBlend = alphaBlend;
        shaderBlit.flipAlpha = false;
        shaderBlit.frame = BdMakePresetFrameInput(ctx, gl, &frameContext);

        if (oldPreset)
            shaderBlit.frame.state = ctx->oldState;

        render_gl::WarpedBlitShaders(shaderBlit, oldPreset ? *gl->oldWarpInputs : *gl->warpInputs, gl->warpMesh,
            gl->warpVerts, gl->targets.Source(), gl->targets.Dest());
    }

    void BdGlWarpFixedPass(sv_context* ctx, BdGlPipeline* gl, bool alphaBlend)
    {
        render_gl::WarpBlitInput blit;
        blit.verts = ctx->meshVerts.data();
        blit.vertexCount = static_cast<int>(ctx->meshVerts.size());
        blit.indexCount = gl->warpIndexCount;
        blit.decay = static_cast<float>(*ctx->state->var_pf_decay);
        blit.wrap = *ctx->state->var_pf_wrap > ctx->snapPoint;
        blit.texSizeX = ctx->texsizeX;
        blit.texSizeY = ctx->texsizeY;
        blit.alphaBlend = alphaBlend;
        blit.flipAlpha = false;

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::WarpedBlitNoShaders(blit, gl->texturedProgram, gl->warpMesh, gl->warpVerts,
            gl->targets.Source(), gl->targets.Dest());
        BdGlSetPixelCenterOffset(ctx, gl, false);
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

        render_core::ComputeGridAlphaValues(input, ctx->meshVerts.data(), ctx->meshVertInfo.data());

        const bool bNewPresetUsesWarpShader = gl->warpShaderActive;

        if (!ctx->state->m_bBlending)
        {
            if (bNewPresetUsesWarpShader)
                BdGlWarpShaderPass(ctx, gl, false, false);
            else
                BdGlWarpFixedPass(ctx, gl, false);
            return;
        }

        const bool bOldPresetUsesWarpShader = gl->oldWarpShaderActive;

        if (bOldPresetUsesWarpShader && bNewPresetUsesWarpShader)
        {
            BdGlWarpShaderPass(ctx, gl, true, false);
            BdGlWarpShaderPass(ctx, gl, false, true);
        }
        else if (!bOldPresetUsesWarpShader && bNewPresetUsesWarpShader)
        {
            BdGlWarpFixedPass(ctx, gl, false);
            BdGlWarpShaderPass(ctx, gl, false, true);
        }
        else if (bOldPresetUsesWarpShader && !bNewPresetUsesWarpShader)
        {
            BdGlWarpShaderPass(ctx, gl, true, false);
            BdGlWarpFixedPass(ctx, gl, true);
        }
        else
        {
            BdGlWarpFixedPass(ctx, gl, false);
        }
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

        BdGlSetPixelCenterOffset(ctx, gl, true);
        render_gl::DrawDarkenCenter(input, gl->untexturedProgram, gl->quadMesh);
        render_gl::DrawBorders(input, gl->untexturedProgram, gl->quadMesh);
        BdGlSetPixelCenterOffset(ctx, gl, false);
    }

    void BdGlCompShaderPass(sv_context* ctx, BdGlPipeline* gl, render_gl::Texture& compSource, float time, bool oldPreset,
        bool alphaBlend, bool flipAlpha)
    {
        const FrameContext frameContext = BdMakeShaderFrameContext(ctx);

        render_gl::ShowToUserShaderInput shaderShow;
        shaderShow.width = ctx->width;
        shaderShow.height = ctx->height;
        shaderShow.time = time;

        for (int i = 0; i < 4; i++)
            shaderShow.randStart[i] = kBeatdropRandStart[i];

        shaderShow.blendVerts = ctx->meshVerts.data();
        shaderShow.blendGridX = ctx->nGridX;
        shaderShow.blendGridY = ctx->nGridY;
        shaderShow.alphaBlend = alphaBlend;
        shaderShow.flipAlpha = flipAlpha;
        shaderShow.frame = BdMakePresetFrameInput(ctx, gl, &frameContext);

        if (oldPreset)
            shaderShow.frame.state = ctx->oldState;

        render_gl::ShowToUserShaders(shaderShow, oldPreset ? *gl->oldCompInputs : *gl->compInputs, gl->compGrid,
            gl->compMesh, compSource);
    }

    void BdGlCompFixedPass(sv_context* ctx, BdGlPipeline* gl, const render_gl::ShowToUserInput& show,
        render_gl::Texture& noShaderSource)
    {
        BdGlSetPixelCenterOffset(gl, true, ctx->width, ctx->height);
        render_gl::ShowToUserNoShaders(show, gl->texturedProgram, gl->untexturedProgram, gl->quadMesh, noShaderSource);
        BdGlSetPixelCenterOffset(gl, false, ctx->width, ctx->height);
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

        if (ctx->state->m_bBlending &&
            ctx->state->m_fVideoEchoAlpha.eval(time) > 0.01f &&
            ctx->state->m_fVideoEchoAlphaOld > 0.01f &&
            ctx->state->m_nVideoEchoOrientation != ctx->state->m_nVideoEchoOrientationOld)
        {
            if (ctx->state->m_fBlendProgress < ctx->snapPoint)
            {
                show.echoOrient = ctx->state->m_nVideoEchoOrientationOld;
                show.echoAlpha *= 1.0f - 2.0f * CosineInterp(ctx->state->m_fBlendProgress);
            }
            else
            {
                show.echoAlpha *= 2.0f * CosineInterp(ctx->state->m_fBlendProgress) - 1.0f;
            }
        }

        render_gl::BindExternalTarget(targetFbo, ctx->width, ctx->height);

        const bool bNewPresetUsesCompShader = gl->compShaderActive;

        if (!ctx->state->m_bBlending)
        {
            if (bNewPresetUsesCompShader)
                BdGlCompShaderPass(ctx, gl, compSource, time, false, false, false);
            else
                BdGlCompFixedPass(ctx, gl, show, noShaderSource);
            return;
        }

        const bool bOldPresetUsesCompShader = gl->oldCompShaderActive;

        if (bOldPresetUsesCompShader && bNewPresetUsesCompShader)
        {
            BdGlCompShaderPass(ctx, gl, compSource, time, true, false, false);
            BdGlCompShaderPass(ctx, gl, compSource, time, false, true, false);
        }
        else if (!bOldPresetUsesCompShader && bNewPresetUsesCompShader)
        {
            BdGlCompFixedPass(ctx, gl, show, noShaderSource);
            BdGlCompShaderPass(ctx, gl, compSource, time, false, true, false);
        }
        else if (bOldPresetUsesCompShader && !bNewPresetUsesCompShader)
        {
            BdGlCompFixedPass(ctx, gl, show, noShaderSource);
            BdGlCompShaderPass(ctx, gl, compSource, time, true, true, true);
        }
        else
        {
            BdGlCompFixedPass(ctx, gl, show, noShaderSource);
        }
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

    void BdGlUploadAudio(BdGlPipeline* gl, sv_context* ctx)
    {
        const bool bOldAudioSampled = ctx->state->m_bBlending &&
            ((gl->oldWarpShaderActive && gl->oldWarpInputs->UsesAudio()) ||
            (gl->oldCompShaderActive && gl->oldCompInputs->UsesAudio()));
        const bool bAudioSampled = (gl->warpShaderActive && gl->warpInputs->UsesAudio()) ||
            (gl->compShaderActive && gl->compInputs->UsesAudio()) || bOldAudioSampled;

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
}

void BdGlPresentFrame(sv_context* ctx, uint32_t targetFbo)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    gl->interpolator.ResetHistory();
    gl->pairOnScreen = false;

    if ((gl->interpolator.Valid() || gl->interpolator.Building()) && ++gl->framesSinceMotion > static_cast<int>(kMotionReleaseSeconds * std::max(ctx->displayFps, 1.0f)))
    {
        gl->interpolator.Destroy();
        gl->framesSinceMotion = 0;
        fprintf(stderr, "synesthesia-vibe: motion interpolator released at frame %d\n", ctx->frame);
    }

    render_gl::SetDefaultDrawState();
    BdGlCompositeToTarget(ctx, gl, targetFbo, gl->targets.Dest().ColorTexture(), gl->targets.Source().ColorTexture());
    gl->presentBlurTexUsed = gl->highestBlurTexUsed;

    gl->glErrorCount += render_gl::DrainGlErrors("BdGlPresentFrame");
}

bool BdGlMotionBuildFrame(sv_context* ctx, uint32_t targetFbo)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr || gl->interpolatorFailed || gl->interpolator.Ready())
        return false;

    if (!gl->interpolator.Building())
        gl->interpolator.BeginCreate(ctx->width, ctx->height, gl->flowPixels);

    const render_gl::MotionInterpolator::BuildState state = gl->interpolator.BuildNext();

    if (state == render_gl::MotionInterpolator::BuildState::Failed)
    {
        gl->interpolatorFailed = true;
        fprintf(stderr, "synesthesia-vibe: motion interpolator unavailable, frames hold below 1x\n");
    }
    else if (state == render_gl::MotionInterpolator::BuildState::Ready)
    {
        fprintf(stderr, "synesthesia-vibe: motion interpolator ready size=%dx%d flow=%dx%d levels=%d\n", ctx->width, ctx->height,
            gl->interpolator.FlowWidth(), gl->interpolator.FlowHeight(), gl->interpolator.FlowLevels());

        if (!gl->interpolator.FlowValid())
            fprintf(stderr, "synesthesia-vibe: motion interpolator flow unavailable, cross-fade below 1x\n");
    }

    BdGlPresentFrame(ctx, targetFbo);
    gl->framesSinceMotion = 0;
    return true;
}

void BdGlNoteHardCut(sv_context* ctx)
{
    if (ctx == nullptr || ctx->gl == nullptr)
        return;

    ctx->gl->interpolator.MarkCut();
}

bool BdGlMotionHistoryEmpty(sv_context* ctx)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr || gl->interpolatorFailed)
        return false;

    return !gl->interpolator.HasHistory();
}

void BdGlPresentMotionFrame(sv_context* ctx, uint32_t targetFbo, bool stepped, float fraction, float framesPerStep)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    if (!gl->interpolator.Ready())
    {
        BdGlPresentFrame(ctx, targetFbo);
        return;
    }

    render_gl::SetDefaultDrawState();
    gl->framesSinceMotion = 0;

    if (stepped || !gl->interpolator.HasHistory())
    {
        BdGlStageComposite(ctx, gl, gl->interpolator.CaptureFramebufferId(), gl->targets.Dest().ColorTexture(),
            gl->targets.Source().ColorTexture());
        gl->interpolator.CommitCapture(static_cast<float>(ctx->timeModel.GetTime()));
        gl->presentBlurTexUsed = gl->highestBlurTexUsed;
    }

    const float fieldEnter = 1.95f;
    const float fieldLeave = 1.8f;
    gl->motionFieldPath = framesPerStep >= (gl->motionFieldPath ? fieldLeave : fieldEnter);

    if (!gl->limiterEnabled)
    {
        gl->interpolator.Present(fraction, targetFbo, gl->motionFieldPath);
    }
    else
    {
        const render_gl::LuminanceLimiterInput input = BdGlLimiterInput(ctx, gl->interpolator.PresentedTime(fraction));

        gl->interpolator.Present(fraction, gl->limiter.SceneFramebufferId(), gl->motionFieldPath);
        gl->limiter.Apply(input, targetFbo);
    }

    render_gl::BindExternalTarget(targetFbo, ctx->width, ctx->height);

    gl->pairOnScreen = true;
    gl->shownFraction = fraction;
    gl->shownFramesPerStep = framesPerStep;

    gl->glErrorCount += render_gl::DrainGlErrors("BdGlPresentMotionFrame");
}

bool BdGlMotionPairOnScreen(sv_context* ctx)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    return gl != nullptr && gl->pairOnScreen && gl->interpolator.HasHistory();
}

bool BdGlMotionFieldPath(sv_context* ctx)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    return gl != nullptr && gl->motionFieldPath;
}

void BdGlRepresentMotionPair(sv_context* ctx, uint32_t targetFbo)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    BdGlPresentMotionFrame(ctx, targetFbo, false, gl->shownFraction, gl->shownFramesPerStep);
}

void BdGlPlayOutMotionPair(sv_context* ctx, uint32_t targetFbo)
{
    BdGlPipeline* gl = BdGlReadyPipeline(ctx);
    if (gl == nullptr)
        return;

    BdGlPresentMotionFrame(ctx, targetFbo, false, 1.0f, gl->shownFramesPerStep);
    gl->interpolator.ResetHistory();
    gl->pairOnScreen = false;
}
