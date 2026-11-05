//
//  custom_waves.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_CUSTOM_WAVES_H__
#define __BEATDROP_RENDER_GL_CUSTOM_WAVES_H__ 1

#include "draw_program.h"
#include "frame_inputs.h"
#include "mesh_buffer.h"
#include "sound_analysis.h"
#include "state.h"

namespace render_gl
{

struct CustomWavesInput
{
    CState* state;
    CState* oldState;
    const FrameInputs* frameInputs;
    const td_soundinfo* sound;
    bool screenDependentRenderMode;
    float invAspectX;
    float invAspectY;
    int texSizeX;
    int texSizeY;
};

void DrawCustomWaves(const CustomWavesInput& input, const DrawProgram& program, MeshBuffer& mesh);

}

#endif
