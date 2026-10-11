//
//  show_to_user_shaders.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_SHOW_TO_USER_SHADERS_H__
#define __BEATDROP_RENDER_GL_SHOW_TO_USER_SHADERS_H__ 1

#include <vector>

#include "mesh.h"
#include "mesh_buffer.h"
#include "preset_shader_inputs.h"
#include "texture.h"

namespace render_gl
{

constexpr int kCompositeGridX = 32;
constexpr int kCompositeGridY = 24;

class CompositeGrid
{
public:
    CompositeGrid();

    bool Build(MeshBuffer& mesh, int viewportWidth, int viewportHeight, bool screenDependentRenderMode,
        float aspectX, float aspectY);

    std::vector<render_core::Vertex>& Vertices() { return m_verts; }
    int IndexCount() const { return m_indexCount; }

private:
    std::vector<render_core::Vertex> m_verts;
    std::vector<int> m_indices;
    int m_indexCount;
};

struct ShowToUserShaderInput
{
    int width;
    int height;
    float time;
    float randStart[4];
    PresetFrameInput frame;
};

void ShowToUserShaders(const ShowToUserShaderInput& input, PresetShaderInputs& shaderInputs, CompositeGrid& grid,
    MeshBuffer& mesh, const Texture& source);

}

#endif
