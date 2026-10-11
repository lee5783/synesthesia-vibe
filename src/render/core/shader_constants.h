//
//  shader_constants.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-02.
//

#ifndef __BEATDROP_RENDER_CORE_SHADER_CONSTANTS_H__
#define __BEATDROP_RENDER_CORE_SHADER_CONSTANTS_H__ 1

#include "frame_context.h"
#include "state.h"

#include <vector>

namespace render_core
{

struct ShaderVec4
{
    float x, y, z, w;
};

constexpr int kShaderConstHandleCount = 17;
constexpr int kShaderConstQVectorCount = (NUM_Q_VAR + 3) / 4;

struct TexSizeConstInput
{
    int w;
    int h;
};

struct ShaderConstantBlock
{
    std::vector<ShaderVec4> texSizeVectors;
    ShaderVec4 randFrame;
    ShaderVec4 randPreset;
    ShaderVec4 consts[kShaderConstHandleCount];
    ShaderVec4 qVectors[kShaderConstQVectorCount];
};

struct ShaderConstantInput
{
    const FrameContext* ctx;
    CState* state;
    ShaderVec4 randFrame;
    int texSizeX;
    int texSizeY;
    std::vector<TexSizeConstInput> texSizeParams;
};

void ComputeShaderConstants(const ShaderConstantInput& input, ShaderConstantBlock& outBlock);

}

#endif
