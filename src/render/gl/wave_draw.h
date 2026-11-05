//
//  wave_draw.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_WAVE_DRAW_H__
#define __BEATDROP_RENDER_GL_WAVE_DRAW_H__ 1

#include "draw_program.h"
#include "mesh_buffer.h"
#include "sound_analysis.h"
#include "state.h"

namespace render_gl
{

struct WaveDrawInput
{
    CState* state;
    const td_mysounddata* mysound;
    float time;
    bool screenDependentRenderMode;
    float aspectX;
    float aspectY;
    int texSizeX;
    int texSizeY;
};

void DrawWave(const WaveDrawInput& input, const float* fL, const float* fR, const DrawProgram& program, MeshBuffer& mesh);

}

#endif
