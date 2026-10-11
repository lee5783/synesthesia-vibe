//
//  noise.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-02.
//

#ifndef __BEATDROP_RENDER_CORE_NOISE_H__
#define __BEATDROP_RENDER_CORE_NOISE_H__ 1

#include "time_model.h"

#include <cstdint>
#include <random>

namespace render_core
{

float CubicInterpolateChannel(float y0, float y1, float y2, float y3, float t);
uint32_t CubicInterpolatePixel(uint32_t y0, uint32_t y1, uint32_t y2, uint32_t y3, float t);

void GenerateNoiseTex(uint32_t* outPixels, int size, int zoomFactor, std::mt19937_64& gen, IRawClock& clock);
void GenerateNoiseVol(uint32_t* outVoxels, int size, int zoomFactor, std::mt19937_64& gen, IRawClock& clock);

}

#endif
