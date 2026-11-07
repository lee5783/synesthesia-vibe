//
//  blend_pattern.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

#ifndef __BEATDROP_RENDER_CORE_BLEND_PATTERN_H__
#define __BEATDROP_RENDER_CORE_BLEND_PATTERN_H__ 1

#include <random>

#include "mesh.h"

namespace render_core
{

struct BlendPatternClock
{
    float checkerTime;
    float tileTime;
    float fireTime;
    float drainTime;
};

struct BlendPatternInput
{
    int pattern;
    int gridX;
    int gridY;
    bool screenDependentRenderMode;
    float aspectX;
    float aspectY;
    float fps;
};

void RandomizeBlendPattern(const BlendPatternInput& input, BlendPatternClock& clock, std::mt19937_64& gen,
    VertInfo* vertInfo);

}

#endif
