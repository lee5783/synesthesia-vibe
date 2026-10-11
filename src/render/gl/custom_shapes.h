//
//  custom_shapes.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_CUSTOM_SHAPES_H__
#define __BEATDROP_RENDER_GL_CUSTOM_SHAPES_H__ 1

#include "draw_program.h"
#include "frame_inputs.h"
#include "mesh_buffer.h"
#include "state.h"
#include "texture.h"

namespace render_gl
{

struct CustomShapesInput
{
    CState* state;
    CState* oldState;
    const FrameInputs* frameInputs;
    bool screenDependentRenderMode;
    float aspectY;
    int texSizeX;
    int texSizeY;
};

void DrawCustomShapes(const CustomShapesInput& input, const DrawProgram& texturedProgram,
    const DrawProgram& untexturedProgram, MeshBuffer& mesh, const Texture& source);

}

#endif
