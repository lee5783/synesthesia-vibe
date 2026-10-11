//
//  shader_constants.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-02.
//

#include "shader_constants.h"

#include "mesh.h"

#include <cmath>

namespace render_core
{

void ComputeShaderConstants(const ShaderConstantInput& input, ShaderConstantBlock& outBlock)
{
    const FrameContext& ctx = *input.ctx;
    CState* pState = input.state;

    outBlock.texSizeVectors.clear();
    outBlock.texSizeVectors.reserve(input.texSizeParams.size());
    for (const TexSizeConstInput& q : input.texSizeParams)
    {
        outBlock.texSizeVectors.push_back(ShaderVec4{
            (float)q.w, (float)q.h, 1.0f/(float)q.w, 1.0f/(float)q.h
        });
    }

    float time_since_preset_start = ctx.time - pState->GetPresetStartTime();
    float time_since_preset_start_wrapped = time_since_preset_start - (int)(time_since_preset_start/10000)*10000;
    (void)time_since_preset_start_wrapped;
    double time = ctx.time - ctx.startTime;
    float progress = (ctx.time - ctx.presetStartTime) / (ctx.nextPresetTime - ctx.presetStartTime);
    float mip_x = logf((float)ctx.pixelsX)/logf(2.0f);
    float mip_y = logf((float)ctx.pixelsX)/logf(2.0f);
    float mip_avg = 0.5f*(mip_x + mip_y);
    float aspect_x = 1;
    float aspect_y = 1;
    float aspect_x_sd = 1;
    float aspect_y_sd = 1;

    if (!ctx.bScreenDependentRenderMode)
    {
        if (ctx.pixelsX > ctx.pixelsY)
            aspect_y_sd = ctx.pixelsY/(float)ctx.pixelsX;
        else
            aspect_x_sd = ctx.pixelsX/(float)ctx.pixelsY;
    }

    if (ctx.pixelsX > ctx.pixelsY)
        aspect_y = ctx.pixelsY/(float)ctx.pixelsX;
    else
        aspect_x = ctx.pixelsX/(float)ctx.pixelsY;

    float blur_min[3], blur_max[3];
    GetSafeBlurMinMax(pState, blur_min, blur_max);

    outBlock.randFrame = input.randFrame;
    outBlock.randPreset = ShaderVec4{
        pState->m_rand_preset.x, pState->m_rand_preset.y, pState->m_rand_preset.z, pState->m_rand_preset.w
    };

    ShaderVec4* c = outBlock.consts;
    c[0] = ShaderVec4{ aspect_x, aspect_y, 1.0f/aspect_x, 1.0f/aspect_y };
    c[1] = ShaderVec4{ aspect_x_sd, aspect_y_sd, 1.0f/aspect_x_sd, 1.0f/aspect_y_sd };
    c[2] = ShaderVec4{ ctx.time, ctx.fps, (float)ctx.frame, progress };
    c[3] = ShaderVec4{ ctx.mysound.imm_rel[0], ctx.mysound.imm_rel[1], ctx.mysound.imm_rel[2], 0.3333f*(ctx.mysound.imm_rel[0], ctx.mysound.imm_rel[1], ctx.mysound.imm_rel[2]) };
    c[4] = ShaderVec4{ ctx.mysound.avg_rel[0], ctx.mysound.avg_rel[1], ctx.mysound.avg_rel[2], 0.3333f*(ctx.mysound.avg_rel[0], ctx.mysound.avg_rel[1], ctx.mysound.avg_rel[2]) };
    c[5] = ShaderVec4{ blur_max[0]-blur_min[0], blur_min[0], blur_max[1]-blur_min[1], blur_min[1] };
    c[6] = ShaderVec4{ blur_max[2]-blur_min[2], blur_min[2], blur_min[0], blur_max[0] };
    c[7] = ShaderVec4{ (float)input.texSizeX, (float)input.texSizeY, 1.0f/(float)input.texSizeX, 1.0f/(float)input.texSizeY };
    c[8] = ShaderVec4{
        0.5f+0.5f*cosf(time* 0.329f+1.2f),
        0.5f+0.5f*cosf(time* 1.293f+3.9f),
        0.5f+0.5f*cosf(time* 5.070f+2.5f),
        0.5f+0.5f*cosf(time*20.051f+5.4f)
    };
    c[9] = ShaderVec4{
        0.5f+0.5f*sinf(time* 0.329f+1.2f),
        0.5f+0.5f*sinf(time* 1.293f+3.9f),
        0.5f+0.5f*sinf(time* 5.070f+2.5f),
        0.5f+0.5f*sinf(time*20.051f+5.4f)
    };
    c[10] = ShaderVec4{
        0.5f+0.5f*cosf(time*0.0050f+2.7f),
        0.5f+0.5f*cosf(time*0.0085f+5.3f),
        0.5f+0.5f*cosf(time*0.0133f+4.5f),
        0.5f+0.5f*cosf(time*0.0217f+3.8f)
    };
    c[11] = ShaderVec4{
        0.5f+0.5f*sinf(time*0.0050f+2.7f),
        0.5f+0.5f*sinf(time*0.0085f+5.3f),
        0.5f+0.5f*sinf(time*0.0133f+4.5f),
        0.5f+0.5f*sinf(time*0.0217f+3.8f)
    };
    c[12] = ShaderVec4{ mip_x, mip_y, mip_avg, 0 };
    c[13] = ShaderVec4{ blur_min[1], blur_max[1], blur_min[2], blur_max[2] };
    c[14] = ShaderVec4{
        ctx.mouseX != -1 ? ctx.mouseX : -1,
        ctx.mouseY != -1 ? -ctx.mouseY + 1 : -1,
        ctx.mouseDown ? ctx.lastMouseX : -ctx.lastMouseX,
        ctx.mouseClicked > 0 ? ctx.lastMouseY : -ctx.lastMouseY
    };
    c[15] = ShaderVec4{ (float)ctx.hour, (float)ctx.minute, (float)ctx.second, ctx.totalSeconds };
    c[16] = ShaderVec4{ (float)ctx.year, (float)ctx.month, (float)ctx.day, (float)ctx.weekday };

    for (int i = 0; i < kShaderConstQVectorCount; i++)
    {
        outBlock.qVectors[i] = ShaderVec4{
            (float)*pState->var_pf_q[i*4+0],
            (float)*pState->var_pf_q[i*4+1],
            (float)*pState->var_pf_q[i*4+2],
            (float)*pState->var_pf_q[i*4+3]
        };
    }
}

}
