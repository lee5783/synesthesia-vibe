//
//  frame_inputs.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_PARSER_FRAME_INPUTS_H__
#define __BEATDROP_PARSER_FRAME_INPUTS_H__ 1

#include "../audio/sound_analysis.h"

struct FrameInputs
{
    const td_mysounddata& mysound;
    float time;
    float fps;
    int   frame;
    float startTime;
    float presetStartTime;
    float nextPresetTime;
    int   gridX;
    int   gridY;
    int   pixelsX;
    int   pixelsY;
    bool  bScreenDependentRenderMode;
    float invAspectX;
    float invAspectY;
    float mouseX;
    float mouseY;
    bool  mouseDown;
    int   mouseClicked;
    bool  ctrlLeft;
    bool  ctrlRight;
    bool  ctrlDown;
    bool  ctrlUp;
    float year;
    float month;
    float day;
    float weekday;
    float hour;
    float minute;
    float second;
    float milliseconds;
    float totalSeconds;
};

#endif
