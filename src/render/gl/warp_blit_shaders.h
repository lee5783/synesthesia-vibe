//
//  warp_blit_shaders.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_WARP_BLIT_SHADERS_H__
#define __BEATDROP_RENDER_GL_WARP_BLIT_SHADERS_H__ 1

#include <vector>

#include "framebuffer.h"
#include "mesh.h"
#include "mesh_buffer.h"
#include "preset_shader_inputs.h"

namespace render_gl
{

struct WarpShaderBlitInput
{
    const render_core::Vertex* verts;
    int vertexCount;
    int gridX;
    int gridY;
    int indexCount;
    PresetFrameInput frame;
};

void WarpedBlitShaders(const WarpShaderBlitInput& input, PresetShaderInputs& shaderInputs, MeshBuffer& mesh,
    std::vector<render_core::Vertex>& scratch, Framebuffer& source, Framebuffer& dest);

}

#endif
