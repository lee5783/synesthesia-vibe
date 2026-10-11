//
//  show_to_user_shaders.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "show_to_user_shaders.h"
#include "blend_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace render_gl
{

namespace
{

uint32_t PackOpaqueColor01(const float (&color)[3])
{
    const auto channel = [](float v)
    {
        return static_cast<uint32_t>(std::max(0.0f, std::min(1.0f, v)) * 255.0f);
    };

    return 0xFF000000u | (channel(color[0]) << 16) | (channel(color[1]) << 8) | channel(color[2]);
}

void ComputeCornerShades(const ShowToUserShaderInput& input, float (&shade)[4][3])
{
    for (int i = 0; i < 4; i++)
    {
        shade[i][0] = 0.6f + 0.3f * sinf(input.time * 30.0f * 0.0143f + 3 + i * 21 + input.randStart[3]);
        shade[i][1] = 0.6f + 0.3f * sinf(input.time * 30.0f * 0.0107f + 1 + i * 13 + input.randStart[1]);
        shade[i][2] = 0.6f + 0.3f * sinf(input.time * 30.0f * 0.0129f + 6 + i * 9 + input.randStart[2]);

        float maxComponent = ((shade[i][0] > shade[i][1]) ? shade[i][0] : shade[i][1]);

        if (shade[i][2] > maxComponent)
            maxComponent = shade[i][2];

        for (int k = 0; k < 3; k++)
        {
            shade[i][k] /= maxComponent;
            shade[i][k] = 0.5f + 0.5f * shade[i][k];
        }
    }
}

void ApplyVertexShades(const float (&shade)[4][3], std::vector<render_core::Vertex>& verts)
{
    for (int j = 0; j < kCompositeGridY; j++)
    {
        for (int i = 0; i < kCompositeGridX; i++)
        {
            render_core::Vertex& vert = verts[static_cast<size_t>(i + j * kCompositeGridX)];
            const float x = vert.x * 0.5f + 0.5f;
            const float y = vert.y * 0.5f + 0.5f;

            float color[3];

            for (int c = 0; c < 3; c++)
            {
                color[c] = shade[0][c] * (x) * (y) +
                           shade[1][c] * (1 - x) * (y) +
                           shade[2][c] * (x) * (1 - y) +
                           shade[3][c] * (1 - x) * (1 - y);
            }

            vert.Diffuse = PackOpaqueColor01(color);
        }
    }
}

}

CompositeGrid::CompositeGrid()
    : m_indexCount(0)
{
}

bool CompositeGrid::Build(MeshBuffer& mesh, int viewportWidth, int viewportHeight, bool screenDependentRenderMode,
    float aspectX, float aspectY)
{
    m_verts.assign(static_cast<size_t>(kCompositeGridX) * static_cast<size_t>(kCompositeGridY), render_core::Vertex());

    m_indexCount = (kCompositeGridX - 2) * (kCompositeGridY - 2) * 2 * 3;
    m_indices.assign(static_cast<size_t>(m_indexCount), 0);

    render_core::BuildCompositeMesh(kCompositeGridX, kCompositeGridY, viewportWidth, viewportHeight,
        screenDependentRenderMode, aspectX, aspectY, m_verts.data(), m_indices.data());

    return mesh.UploadIndices(m_indices.data(), m_indexCount);
}

void ShowToUserShaders(const ShowToUserShaderInput& input, PresetShaderInputs& shaderInputs, CompositeGrid& grid,
    MeshBuffer& mesh, const Texture& source)
{
    float shade[4][3] = {
        { 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f } };

    ComputeCornerShades(input, shade);
    ApplyVertexShades(shade, grid.Vertices());

    SetBlendOpaque();

    PresetFrameInput frame = input.frame;
    frame.mainTexture = source.Id();

    shaderInputs.Apply(frame);
    SetPresetPixelCenterOffset(shaderInputs.PosOffsetLocation(), input.width, input.height);

    mesh.UploadVertices(grid.Vertices().data(), static_cast<int>(grid.Vertices().size()));
    mesh.DrawIndexedTriangles(grid.IndexCount());

    shaderInputs.Restore();
}

}
