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

        render_gl::WarpBlitInput blit;
        blit.verts = ctx->meshVerts.data();
        blit.vertexCount = static_cast<int>(ctx->meshVerts.size());
        blit.indexCount = gl->warpIndexCount;
        blit.decay = static_cast<float>(*ctx->state->var_pf_decay);
        blit.wrap = *ctx->state->var_pf_wrap > ctx->snapPoint;
        blit.texSizeX = ctx->texsizeX;
        blit.texSizeY = ctx->texsizeY;

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
        const int levelsUsed = std::max(gl->highestBlurTexUsed, gl->forcedBlurLevels);
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

    void BdGlStageComposite(sv_context* ctx, BdGlPipeline* gl, uint32_t targetFbo)
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

            render_gl::ShowToUserShaders(shaderShow, gl->compInputs, gl->compGrid, gl->compMesh,
                gl->targets.Source().ColorTexture());
            return;
        }

        BdGlSetPixelCenterOffset(gl, true, ctx->width, ctx->height);
        render_gl::ShowToUserNoShaders(show, gl->texturedProgram, gl->untexturedProgram, gl->quadMesh,
            gl->targets.Dest().ColorTexture());
        BdGlSetPixelCenterOffset(gl, false, ctx->width, ctx->height);
    }
}

void BdGlRenderFrame(sv_context* ctx, uint32_t targetFbo)
{
    if (ctx == nullptr || ctx->gl == nullptr || !ctx->gl->ready)
        return;

    BdGlPipeline* gl = ctx->gl;

    render_gl::SetDefaultDrawState();

    const bool bAudioSampled = (gl->warpShaderActive && gl->warpInputs.UsesAudio()) ||
        (gl->compShaderActive && gl->compInputs.UsesAudio());

    if (bAudioSampled)
        gl->audio.Upload(ctx->fftSmoothed, ctx->fftPeak, ctx->sound.fWaveform[0], ctx->sound.fWaveform[1]);

    BdGlStageMotionVectors(ctx, gl);
    BdGlStageFirstFrameClear(ctx, gl);
    BdGlStageWarp(ctx, gl);
    BdGlStageBlur(ctx, gl);
    BdGlStageCustomShapes(ctx, gl);
    BdGlStageCustomWaves(ctx, gl);
    BdGlStageWave(ctx, gl);
    BdGlStageBorders(ctx, gl);
    BdGlStageComposite(ctx, gl, targetFbo);

    gl->targets.Swap();
    ctx->framesSinceResize++;

    gl->glErrorCount += render_gl::DrainGlErrors("BdGlRenderFrame");
}
