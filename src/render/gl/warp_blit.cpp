//
//  warp_blit.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "warp_blit.h"
#include "blend_state.h"

#include <algorithm>
#include <cmath>

namespace render_gl
{

uint32_t PackDecayColor(float decay)
{
    const uint32_t channel = static_cast<uint32_t>(static_cast<int>(decay * 255.0f)) & 0xFFu;

    return (channel << 16) | (channel << 8) | channel;
}

float DecayForStep(float decay, float stepFraction)
{
    const float packed = static_cast<float>(PackDecayColor(decay) & 0xFFu) / 255.0f;

    return powf(packed, stepFraction);
}

void WarpedBlitNoShaders(const WarpBlitInput& input, const DrawProgram& program, MeshBuffer& mesh,
    std::vector<render_core::Vertex>& scratch, Framebuffer& source, Framebuffer& dest)
{
    const bool bFractionalStep = input.stepFraction != 1.0f;
    const uint32_t decayColor = bFractionalStep ? PackDecayColor(1.0f) : PackDecayColor(input.decay);
    const size_t vertexCount = std::min(static_cast<size_t>(input.vertexCount), scratch.size());

    for (size_t i = 0; i < vertexCount; i++)
    {
        render_core::Vertex vert = input.verts[i];
        vert.y *= -1.0f;
        vert.Diffuse = decayColor | (vert.Diffuse & 0xFF000000u);
        scratch[i] = vert;
    }

    source.ColorTexture().SetWrapRepeat(input.wrap);
    dest.Bind();

    if (bFractionalStep)
        SetBlendConstantScale(DecayForStep(input.decay, input.stepFraction));
    else
        SetBlendOpaque();

    program.Bind();
    program.SetUvOffset(0.0f, 0.0f);

    source.ColorTexture().Bind();

    mesh.UploadVertices(scratch.data(), static_cast<int>(vertexCount));
    mesh.DrawIndexedTriangles(input.indexCount);

    program.SetUvOffset(0.0f, 0.0f);

    if (bFractionalStep)
        SetBlendOpaque();
}

}
