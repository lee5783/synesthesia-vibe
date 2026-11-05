//
//  motion_vectors.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_MOTION_VECTORS_H__
#define __BEATDROP_RENDER_GL_MOTION_VECTORS_H__ 1

#include "draw_program.h"
#include "mesh.h"
#include "mesh_buffer.h"
#include "state.h"

namespace render_gl
{

struct MotionVectorsInput
{
    CState* state;
    const render_core::Vertex* verts;
    int gridX;
    int gridY;
    int texSizeX;
};

void DrawMotionVectors(const MotionVectorsInput& input, const DrawProgram& program, MeshBuffer& mesh);

}

#endif
