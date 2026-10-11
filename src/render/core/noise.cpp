//
//  noise.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-02.
//

#include "noise.h"

#include "rng_compat.h"

namespace render_core
{

namespace
{
    struct TickSplit
    {
        uint32_t low;
        int32_t high;
    };

    TickSplit SplitTicks(long long ticks)
    {
        TickSplit s;
        s.low = (uint32_t)(unsigned long long)ticks;
        s.high = (int32_t)(uint32_t)((unsigned long long)ticks >> 32);
        return s;
    }
}

float CubicInterpolateChannel(float y0, float y1, float y2, float y3, float t)
{
   float a0,a1,a2,a3,t2;

   t2 = t*t;
   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;
   a3 = y1;

   return(a0*t*t2+a1*t2+a2*t+a3);
}

uint32_t CubicInterpolatePixel(uint32_t y0, uint32_t y1, uint32_t y2, uint32_t y3, float t)
{
    // performs cubic interpolation on a packed BGRA value.
    uint32_t ret = 0;
    uint32_t shift = 0;
    for (int i=0; i<4; i++)
    {
        float f = CubicInterpolateChannel(
            ((y0 >> shift) & 0xFF)/255.0f,
            ((y1 >> shift) & 0xFF)/255.0f,
            ((y2 >> shift) & 0xFF)/255.0f,
            ((y3 >> shift) & 0xFF)/255.0f,
            t
        );
        if (f<0)
            f = 0;
        if (f>1)
            f = 1;
        ret |= ((uint32_t)(f*255)) << shift;
        shift += 8;
    }
    return ret;
}

void GenerateNoiseTex(uint32_t* outPixels, int size, int zoomFactor, std::mt19937_64& gen, IRawClock& clock)
{
    // size = width & height of the texture;
    // zoomFactor = how zoomed-in the texture features should be.
    //           1 = random noise
    //           2 = smoothed (interp)
    //           4/8/16... = cubic interp.

    int dwords_per_line = size;
    int RANGE = (zoomFactor > 1) ? 216 : 256;
    BdUniformIntDistribution<int> range_dist(0, RANGE - 1);
    BdUniformIntDistribution<int> size_dist(0, size - 1);

    uint32_t* dst = outPixels;
    for (int y=0; y<size; y++) {
        long long ticks = 0;
        clock.QueryHighPrecisionTicks(ticks);
        TickSplit q = SplitTicks(ticks);
        gen.seed((uint64_t)(q.low ^ (uint32_t)q.high) ^ gen());
        for (int x=0; x<size; x++) {
            dst[x] = ((uint32_t)(range_dist(gen) + RANGE / 2) << 24) |
                     ((uint32_t)(range_dist(gen) + RANGE / 2) << 16) |
                     ((uint32_t)(range_dist(gen) + RANGE / 2) <<  8) |
                     ((uint32_t)(range_dist(gen) + RANGE / 2)      );
        }
        // swap some pixels randomly, to improve 'randomness'
        for (int x=0; x<size; x++)
        {
            int x1 = (int)(((uint32_t)size_dist(gen) ^ q.low) % (uint32_t)size);
            int x2 = (size_dist(gen) ^ q.high) % size;
            uint32_t temp = dst[x2];
            dst[x2] = dst[x1];
            dst[x1] = temp;
        }
        dst += dwords_per_line;
    }

    // smoothing
    if (zoomFactor > 1)
    {
        // first go ACROSS, blending cubically on X, but only on the main lines.
        uint32_t* dst = outPixels;
        for (int y=0; y<size; y+=zoomFactor)
            for (int x=0; x<size; x++)
                if (x % zoomFactor)
                {
                    int base_x = (x/zoomFactor)*zoomFactor + size;
                    int base_y = y*dwords_per_line;
                    uint32_t y0 = dst[ base_y + ((base_x - zoomFactor  ) % size) ];
                    uint32_t y1 = dst[ base_y + ((base_x                ) % size) ];
                    uint32_t y2 = dst[ base_y + ((base_x + zoomFactor  ) % size) ];
                    uint32_t y3 = dst[ base_y + ((base_x + zoomFactor*2) % size) ];

                    float t = (x % zoomFactor)/(float)zoomFactor;

                    uint32_t result = CubicInterpolatePixel(y0, y1, y2, y3, t);

                    dst[ y*dwords_per_line + x ] = result;
                }

        // next go down, doing cubic interp along Y, on every line.
        for (int x=0; x<size; x++)
            for (int y=0; y<size; y++)
                if (y % zoomFactor)
                {
                    int base_y = (y/zoomFactor)*zoomFactor + size;
                    uint32_t y0 = dst[ ((base_y - zoomFactor  ) % size)*dwords_per_line + x ];
                    uint32_t y1 = dst[ ((base_y                ) % size)*dwords_per_line + x ];
                    uint32_t y2 = dst[ ((base_y + zoomFactor  ) % size)*dwords_per_line + x ];
                    uint32_t y3 = dst[ ((base_y + zoomFactor*2) % size)*dwords_per_line + x ];

                    float t = (y % zoomFactor)/(float)zoomFactor;

                    uint32_t result = CubicInterpolatePixel(y0, y1, y2, y3, t);

                    dst[ y*dwords_per_line + x ] = result;
                }

    }
}

void GenerateNoiseVol(uint32_t* outVoxels, int size, int zoomFactor, std::mt19937_64& gen, IRawClock& clock)
{
    // size = width & height & depth of the texture;
    // zoomFactor = how zoomed-in the texture features should be.
    //           1 = random noise
    //           2 = smoothed (interp)
    //           4/8/16... = cubic interp.

    int dwords_per_slice = size*size;
    int dwords_per_line = size;
    int RANGE = (zoomFactor > 1) ? 216 : 256;
    BdUniformIntDistribution<int> range_dist(0, RANGE - 1);
    BdUniformIntDistribution<int> size_dist(0, size - 1);

    for (int z=0; z<size; z++) {
        uint32_t* dst = outVoxels + z*dwords_per_slice;
        for (int y=0; y<size; y++) {
            long long ticks = 0;
            clock.QueryHighPrecisionTicks(ticks);
            TickSplit q = SplitTicks(ticks);
            gen.seed((uint64_t)(q.low ^ (uint32_t)q.high) ^ gen());
            for (int x=0; x<size; x++) {
                dst[x] = ((uint32_t)(range_dist(gen) + RANGE / 2) << 24) |
                         ((uint32_t)(range_dist(gen) + RANGE / 2) << 16) |
                         ((uint32_t)(range_dist(gen) + RANGE / 2) <<  8) |
                         ((uint32_t)(range_dist(gen) + RANGE / 2)      );
            }
            // swap some pixels randomly, to improve 'randomness'
            for (int x=0; x<size; x++)
            {
                int x1 = (int)(((uint32_t)size_dist(gen) ^ q.low) % (uint32_t)size);
                int x2 = (size_dist(gen) ^ q.high) % size;
                uint32_t temp = dst[x2];
                dst[x2] = dst[x1];
                dst[x1] = temp;
            }
            dst += dwords_per_line;
        }
    }

    // smoothing
    if (zoomFactor > 1)
    {
        // first go ACROSS, blending cubically on X, but only on the main lines.
        uint32_t* dst = outVoxels;
        for (int z=0; z<size; z+=zoomFactor)
            for (int y=0; y<size; y+=zoomFactor)
                for (int x=0; x<size; x++)
                    if (x % zoomFactor)
                    {
                        int base_x = (x/zoomFactor)*zoomFactor + size;
                        int base_y = z*dwords_per_slice + y*dwords_per_line;
                        uint32_t y0 = dst[ base_y + ((base_x - zoomFactor  ) % size) ];
                        uint32_t y1 = dst[ base_y + ((base_x                ) % size) ];
                        uint32_t y2 = dst[ base_y + ((base_x + zoomFactor  ) % size) ];
                        uint32_t y3 = dst[ base_y + ((base_x + zoomFactor*2) % size) ];

                        float t = (x % zoomFactor)/(float)zoomFactor;

                        uint32_t result = CubicInterpolatePixel(y0, y1, y2, y3, t);

                        dst[ z*dwords_per_slice + y*dwords_per_line + x ] = result;
                    }

        // next go down, doing cubic interp along Y, on the main slices.
        for (int z=0; z<size; z+=zoomFactor)
            for (int x=0; x<size; x++)
                for (int y=0; y<size; y++)
                    if (y % zoomFactor)
                    {
                        int base_y = (y/zoomFactor)*zoomFactor + size;
                        int base_z = z*dwords_per_slice;
                        uint32_t y0 = dst[ ((base_y - zoomFactor  ) % size)*dwords_per_line + base_z + x ];
                        uint32_t y1 = dst[ ((base_y                ) % size)*dwords_per_line + base_z + x ];
                        uint32_t y2 = dst[ ((base_y + zoomFactor  ) % size)*dwords_per_line + base_z + x ];
                        uint32_t y3 = dst[ ((base_y + zoomFactor*2) % size)*dwords_per_line + base_z + x ];

                        float t = (y % zoomFactor)/(float)zoomFactor;

                        uint32_t result = CubicInterpolatePixel(y0, y1, y2, y3, t);

                        dst[ y*dwords_per_line + base_z + x ] = result;
                    }

        // next go through, doing cubic interp along Z, everywhere.
        for (int x=0; x<size; x++)
            for (int y=0; y<size; y++)
                for (int z=0; z<size; z++)
                    if (z % zoomFactor)
                    {
                        int base_y = y*dwords_per_line;
                        int base_z = (z/zoomFactor)*zoomFactor + size;
                        uint32_t y0 = dst[ ((base_z - zoomFactor  ) % size)*dwords_per_slice + base_y + x ];
                        uint32_t y1 = dst[ ((base_z                ) % size)*dwords_per_slice + base_y + x ];
                        uint32_t y2 = dst[ ((base_z + zoomFactor  ) % size)*dwords_per_slice + base_y + x ];
                        uint32_t y3 = dst[ ((base_z + zoomFactor*2) % size)*dwords_per_slice + base_y + x ];

                        float t = (z % zoomFactor)/(float)zoomFactor;

                        uint32_t result = CubicInterpolatePixel(y0, y1, y2, y3, t);

                        dst[ z*dwords_per_slice + base_y + x ] = result;
                    }
    }
}

}
