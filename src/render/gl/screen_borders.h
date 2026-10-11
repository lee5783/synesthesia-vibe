//
//  screen_borders.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_SCREEN_BORDERS_H__
#define __BEATDROP_RENDER_GL_SCREEN_BORDERS_H__ 1

#include "draw_program.h"
#include "mesh.h"
#include "mesh_buffer.h"
#include "state.h"

namespace render_gl
{

struct BorderDrawInput
{
    CState* state;
    bool screenDependentRenderMode;
    float aspectY;
};

void DrawDarkenCenter(const BorderDrawInput& input, const DrawProgram& program, MeshBuffer& mesh);

void DrawBorders(const BorderDrawInput& input, const DrawProgram& program, MeshBuffer& mesh);

}

#endif
