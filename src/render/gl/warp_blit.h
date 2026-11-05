//
//  warp_blit.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_WARP_BLIT_H__
#define __BEATDROP_RENDER_GL_WARP_BLIT_H__ 1

#include <cstdint>
#include <vector>

#include "draw_program.h"
#include "framebuffer.h"
#include "mesh.h"
#include "mesh_buffer.h"

namespace render_gl
{

struct WarpBlitInput
{
    const render_core::Vertex* verts;
    int vertexCount;
    int indexCount;
    float decay;
    bool wrap;
    int texSizeX;
    int texSizeY;
};

uint32_t PackDecayColor(float decay);

void WarpedBlitNoShaders(const WarpBlitInput& input, const DrawProgram& program, MeshBuffer& mesh,
    std::vector<render_core::Vertex>& scratch, Framebuffer& source, Framebuffer& dest);

}

#endif
