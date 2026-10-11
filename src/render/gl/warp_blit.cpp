//
//  warp_blit.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "warp_blit.h"
#include "blend_state.h"

#include <algorithm>

namespace render_gl
{

uint32_t PackDecayColor(float decay)
{
    const uint32_t channel = static_cast<uint32_t>(static_cast<int>(decay * 255.0f)) & 0xFFu;

    return (channel << 16) | (channel << 8) | channel;
}

void WarpedBlitNoShaders(const WarpBlitInput& input, const DrawProgram& program, MeshBuffer& mesh,
    std::vector<render_core::Vertex>& scratch, Framebuffer& source, Framebuffer& dest)
{
    const uint32_t decayColor = PackDecayColor(input.decay);
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

    SetBlendOpaque();

    program.Bind();
    program.SetUvOffset(0.0f, 0.0f);

    source.ColorTexture().Bind();

    mesh.UploadVertices(scratch.data(), static_cast<int>(vertexCount));
    mesh.DrawIndexedTriangles(input.indexCount);

    program.SetUvOffset(0.0f, 0.0f);
}

}
