//
//  warp_blit_shaders.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "warp_blit_shaders.h"
#include "blend_state.h"

#include <algorithm>

namespace render_gl
{

namespace
{

constexpr float kAngleWrapSeam = 3.1415926535897932384626433832795f;

void SetSeamAngles(std::vector<render_core::Vertex>& scratch, int gridX, int gridY, float angle)
{
    const size_t rowOffset = static_cast<size_t>(gridY / 2) * static_cast<size_t>(gridX + 1);

    for (int x = 0; x < gridX / 2; x++)
    {
        const size_t index = rowOffset + static_cast<size_t>(x);

        if (index < scratch.size())
            scratch[index].ang = angle;
    }
}

}

void WarpedBlitShaders(const WarpShaderBlitInput& input, PresetShaderInputs& shaderInputs, MeshBuffer& mesh,
    std::vector<render_core::Vertex>& scratch, Framebuffer& source, Framebuffer& dest)
{
    const size_t vertexCount = std::min(static_cast<size_t>(input.vertexCount), scratch.size());

    std::copy(input.verts, input.verts + vertexCount, scratch.begin());

    dest.Bind();
    SetBlendOpaque();

    PresetFrameInput frame = input.frame;
    frame.mainTexture = source.ColorTexture().Id();

    shaderInputs.Apply(frame);
    SetPresetPixelCenterOffset(shaderInputs.PosOffsetLocation(), input.frame.texSizeX, input.frame.texSizeY);

    const int halfIndexCount = input.indexCount / 2;

    for (int half = 0; half < 2; half++)
    {
        SetSeamAngles(scratch, input.gridX, input.gridY, half != 0 ? kAngleWrapSeam : -kAngleWrapSeam);

        mesh.UploadVertices(scratch.data(), static_cast<int>(vertexCount));
        mesh.DrawIndexedTriangles(halfIndexCount, half * halfIndexCount);
    }

    shaderInputs.Restore();
}

}
