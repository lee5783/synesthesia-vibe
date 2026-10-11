//
//  show_to_user.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_SHOW_TO_USER_H__
#define __BEATDROP_RENDER_GL_SHOW_TO_USER_H__ 1

#include "draw_program.h"
#include "mesh_buffer.h"
#include "texture.h"

namespace render_gl
{

struct ShowToUserInput
{
    int width;
    int height;
    float invAspectY;
    bool screenDependentRenderMode;
    bool wrap;

    float time;
    float shaderAmount;
    float randStart[4];

    float echoZoom;
    float echoAlpha;
    int echoOrient;
    float gamma;

    bool brighten;
    bool darken;
    bool solarize;
    bool invert;
};

void ShowToUserNoShaders(const ShowToUserInput& input, const DrawProgram& textured,
    const DrawProgram& untextured, MeshBuffer& quad, Texture& source);

}

#endif
