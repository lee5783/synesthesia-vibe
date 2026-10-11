//
//  frame_context.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_RENDER_DX9_FRAME_CONTEXT_H__
#define __BEATDROP_RENDER_DX9_FRAME_CONTEXT_H__ 1

#include "sound_analysis.h"

struct FrameContext
{
    const td_mysounddata& mysound;
    float time;
    float fps;
    int   frame;
    float startTime;
    float presetStartTime;
    float nextPresetTime;
    int   pixelsX;
    int   pixelsY;
    bool  bScreenDependentRenderMode;
    float mouseX;
    float mouseY;
    float lastMouseX;
    float lastMouseY;
    bool  mouseDown;
    int   mouseClicked;
    float year;
    float month;
    float day;
    float weekday;
    float hour;
    float minute;
    float second;
    float totalSeconds;
};

#endif
