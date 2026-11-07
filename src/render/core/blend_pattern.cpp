//
//  blend_pattern.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-07.
//

#include "blend_pattern.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

#include "rng_compat.h"

namespace render_core
{

namespace
{

constexpr int kBlendPatternLimit = 18;

float Frand(std::mt19937_64& gen)
{
    return BdUniformRealDistribution<float>(0.0f, 1.0f)(gen);
}

void GenPlasma(const BlendPatternInput& input, std::mt19937_64& gen, VertInfo* vertInfo, int x0, int x1, int y0, int y1, float dt)
{
    const int gridX = input.gridX;
    const int gridY = input.gridY;
    const bool screenDependentRenderMode = input.screenDependentRenderMode;
    const float aspectX = input.aspectX;
    const float aspectY = input.aspectY;

    int midx = (x0+x1)/2;
    int midy = (y0+y1)/2;
    float t00 = vertInfo[y0*(gridX+1) + x0].c;
    float t01 = vertInfo[y0*(gridX+1) + x1].c;
    float t10 = vertInfo[y1*(gridX+1) + x0].c;
    float t11 = vertInfo[y1*(gridX+1) + x1].c;

    if (y1-y0 >= 2)
    {
        if (x0 == 0)
        {
            if (screenDependentRenderMode)
                vertInfo[midy * (gridX + 1) + x0].c = 0.5f * (t00 + t10) + (Frand(gen) * 2 - 1) * dt;
            else
                vertInfo[midy * (gridX + 1) + x0].c = 0.5f * (t00 + t10) + (Frand(gen) * 2 - 1) * dt * aspectY;
        }
        if (screenDependentRenderMode)
            vertInfo[midy * (gridX + 1) + x1].c = 0.5f * (t01 + t11) + (Frand(gen) * 2 - 1) * dt;
        else
            vertInfo[midy * (gridX + 1) + x1].c = 0.5f * (t01 + t11) + (Frand(gen) * 2 - 1) * dt * aspectY;
    }
    if (x1-x0 >= 2)
    {
        if (y0 == 0)
        {
            if (screenDependentRenderMode)
                vertInfo[y0 * (gridX + 1) + midx].c = 0.5f * (t00 + t01) + (Frand(gen) * 2 - 1) * dt;
            else
                vertInfo[y0 * (gridX + 1) + midx].c = 0.5f * (t00 + t01) + (Frand(gen) * 2 - 1) * dt * aspectX;
        }
        if (screenDependentRenderMode)
            vertInfo[y1 * (gridX + 1) + midx].c = 0.5f * (t10 + t11) + (Frand(gen) * 2 - 1) * dt;
        else
            vertInfo[y1 * (gridX + 1) + midx].c = 0.5f * (t10 + t11) + (Frand(gen) * 2 - 1) * dt * aspectX;
    }

    if (y1-y0 >= 2 && x1-x0 >= 2)
    {
        t00 = vertInfo[midy*(gridX+1) + x0].c;
        t01 = vertInfo[midy*(gridX+1) + x1].c;
        t10 = vertInfo[y0*(gridX+1) + midx].c;
        t11 = vertInfo[y1*(gridX+1) + midx].c;
        vertInfo[midy*(gridX+1) + midx].c = 0.25f*(t10 + t11 + t00 + t01) + (Frand(gen)*2-1)*dt;

        GenPlasma(input, gen, vertInfo, x0, midx, y0, midy, dt*0.5f);
        GenPlasma(input, gen, vertInfo, midx, x1, y0, midy, dt*0.5f);
        GenPlasma(input, gen, vertInfo, x0, midx, midy, y1, dt*0.5f);
        GenPlasma(input, gen, vertInfo, midx, x1, midy, y1, dt*0.5f);
    }
}

}

void RandomizeBlendPattern(const BlendPatternInput& input, BlendPatternClock& clock, std::mt19937_64& gen, VertInfo* vertInfo)
{
    if (!vertInfo)
        return;

    const int gridX = input.gridX;
    const int gridY = input.gridY;
    const bool screenDependentRenderMode = input.screenDependentRenderMode;
    const float aspectX = input.aspectX;
    const float aspectY = input.aspectY;

    BdUniformIntDistribution<int> dist(0, kBlendPatternLimit);
    int mixtype = input.pattern > -1 ? input.pattern : dist(gen);

    if (mixtype==0)
    {
        int nVert = 0;
        for (int y=0; y<=gridY; y++)
        {
            for (int x=0; x<=gridX; x++)
            {
                vertInfo[nVert].a = 1;
                vertInfo[nVert].c = 0;
                nVert++;
            }
        }
    }
    else if (mixtype==1)
    {
        float ang = Frand(gen)*6.28f;
        float vx = cosf(ang);
        float vy = sinf(ang);
        float band = 0.1f + 0.2f*Frand(gen);
        float inv_band = 1.0f/band;

        int nVert = 0;
        for (int y=0; y<=gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY);
            else
                fy = (y / (float)gridY) * aspectY;
            for (int x=0; x<=gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX);
                else
                    fx = (x / (float)gridX) * aspectX;

                float t = (fx-0.5f)*vx + (fy-0.5f)*vy + 0.5f;
                t = (t-0.5f)/sqrtf(2.0f) + 0.5f;

                vertInfo[nVert].a = inv_band * (1 + band);
                vertInfo[nVert].c = -inv_band + inv_band*t;
                nVert++;
            }
        }
    }
    else if (mixtype==2)
    {
        float band = 0.12f + 0.13f*Frand(gen);
        float inv_band = 1.0f/band;

        vertInfo[                               0].c = Frand(gen);
        vertInfo[                        gridX].c = Frand(gen);
        vertInfo[gridY*(gridX+1)           ].c = Frand(gen);
        vertInfo[gridY*(gridX+1) + gridX].c = Frand(gen);
        GenPlasma(input, gen, vertInfo, 0, gridX, 0, gridY, 0.25f);

        float minc = vertInfo[0].c;
        float maxc = vertInfo[0].c;
        int x,y,nVert;

        nVert = 0;
        for (y=0; y<=gridY; y++)
        {
            for (x=0; x<=gridX; x++)
            {
                if (minc > vertInfo[nVert].c)
                    minc = vertInfo[nVert].c;
                if (maxc < vertInfo[nVert].c)
                    maxc = vertInfo[nVert].c;
                nVert++;
            }
        }

        float mult = 1.0f/(maxc-minc);
        nVert = 0;
        for (y=0; y<=gridY; y++)
        {
            for (x=0; x<=gridX; x++)
            {
                float t = (vertInfo[nVert].c - minc)*mult;
                vertInfo[nVert].a = inv_band * (1 + band);
                vertInfo[nVert].c = -inv_band + inv_band*t;
                nVert++;
            }
        }
    }
    else if (mixtype==3)
    {
        const float bandDraw0 = Frand(gen);
        const float bandDraw1 = Frand(gen);
        float band = 0.02f + 0.14f*bandDraw0 + 0.34f*bandDraw1;
        float inv_band = 1.0f/band;
        float dir = (float)((BdRand()%2)*2 - 1);

        int nVert = 0;
        for (int y=0; y<=gridY; y++)
        {
            float dy;
            if (screenDependentRenderMode)
                dy = (y / (float)gridY - 0.5f);
            else
                dy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x=0; x<=gridX; x++)
            {
                float dx;
                if (screenDependentRenderMode)
                    dx = (x / (float)gridX - 0.5f);
                else
                    dx = (x / (float)gridX - 0.5f) * aspectX;
                float t = sqrtf(dx*dx + dy*dy)*1.41421f;
                if (dir==-1)
                    t = 1-t;

                vertInfo[nVert].a = inv_band * (1 + band);
                vertInfo[nVert].c = -inv_band + inv_band*t;
                nVert++;
            }
        }
    }
    else if (mixtype == 4)
    {
        float band = 0.08f + 0.14f * Frand(gen);
        float inv_band = 1.0f / band;
        float dir = (BdRand() % 2) ? 1.0f : -1.0f;
        float start_angle = Frand(gen) * 6.2831853f;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float angle = atan2f(fy, fx);
                float dist = sqrtf(fx * fx + fy * fy) * 1.41421356f;

                if (angle < 0) angle += 6.2831853f;
                angle = fmodf(angle * dir + start_angle + 10.0f * 6.2831853f, 6.2831853f);

                float t = angle / 6.2831853f;
                float t_adjusted = t;

                if (t < band) {
                    t_adjusted = t + 1.0f;
                }

                float blend = (t_adjusted - dist * 0.1f);

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * blend;
                nVert++;
            }
        }
    }
    else if (mixtype == 5)
    {
        float band = 0.07f + 0.1f * Frand(gen);
        float inv_band = 1.0f / band;
        int loops = 2 + (BdRand() % 7);
        float rotation_speed = Frand(gen) * 0.5f;
        bool inward_spiral = (BdRand() % 2) == 0;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float angle = atan2f(fy, fx);
                float radius = sqrtf(fx * fx + fy * fy) * 1.41421356f;

                if (angle < 0) angle += 6.2831853f;

                float spiral_progress = fmodf(angle / (6.2831853f) + loops * radius + rotation_speed, 1.0f);

                if (inward_spiral) {
                    spiral_progress = 1.0f - spiral_progress;
                }

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * spiral_progress;
                nVert++;
            }
        }
    }
    else if (mixtype == 6)
    {
        float band = 0.07f + 0.12f * Frand(gen);
        float inv_band = 1.0f / band;
        float angle = Frand(gen) * 6.2831853f;
        float aspect = 0.8f + Frand(gen) * 2.4f;
        bool reverse = (BdRand() % 2) == 0;

        float cos_a = cosf(angle);
        float sin_a = sinf(angle);
        float norm_factor = 1.0f / (1.0f + aspect);

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float rx = fx * cos_a - fy * sin_a;
                float ry = fx * sin_a + fy * cos_a;

                float diamond = (fabsf(rx) * aspect + fabsf(ry)) * norm_factor;

                float t = reverse ? (1.0f - diamond) : diamond;

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 7)
    {
        float band = 0.05f + 0.15f * Frand(gen);
        float inv_band = 1.0f / band;
        const int exact_repeats = 3;
        bool reverse_direction = (BdRand() % 2) == 0;
        float glow_intensity = 0.5f + Frand(gen) * 1.5f;

        float center_x = 0.5f + (Frand(gen) - 0.5f) * 0.1f;
        float center_y = 0.5f + (Frand(gen) - 0.5f) * 0.1f;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - center_y);
            else
                fy = (y / (float)gridY - center_y) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - center_x);
                else
                    fx = (x / (float)gridX - center_x) * aspectX;

                float angle = atan2f(fy, fx);
                float dist = sqrtf(fx * fx + fy * fy) * 1.41421356f;

                if (angle < 0) angle += 6.2831853f;

                float clock_pos = angle / 6.2831853f * exact_repeats;

                if (reverse_direction)
                    clock_pos = exact_repeats - clock_pos;

                clock_pos = clock_pos - floorf(clock_pos);

                float t = clock_pos;

                float glow = (1.0f - dist) * glow_intensity;
                t += glow * 0.3f;

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 8)
    {
        float band = 0.08f + 0.12f * Frand(gen);
        float inv_band = 1.0f / band;
        bool diagonal = (BdRand() % 2) == 0;
        float center_bias = 0.3f + Frand(gen) * 0.4f;
        float softness = 0.1f + Frand(gen) * 0.2f;
        bool reverse = (BdRand() % 2) == 0;

        auto clamp = [](float value, float min, float max) {
            return (value < min) ? min : ((value > max) ? max : value);
            };

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float t;
                if (diagonal)
                {
                    float d1 = (fx + fy) * 0.7071f;
                    float d2 = (fx - fy) * 0.7071f;
                    t = (fabsf(d1) > fabsf(d2)) ? fabsf(d1) : fabsf(d2);
                }
                else
                {
                    t = (fabsf(fx) > fabsf(fy)) ? fabsf(fx) : fabsf(fy);
                }

                t = powf(t, center_bias);

                t = t * (1.0f + softness) - softness * 0.5f;
                t = clamp(t, 0.0f, 1.0f);

                if (reverse)
                    t = 1.0f - t;

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 9)
    {
        float band = 0.05f + 0.15f * Frand(gen);
        float inv_band = 1.0f / band;
        int checker_size = 4 + (BdRand() % 12);
        float anim_speed = 0.5f + Frand(gen) * 2.0f;
        bool diagonal_anim = (BdRand() % 2) == 0;
        bool reverse = (BdRand() % 2) == 0;

        clock.checkerTime += 1 / input.fps;
        float time = clock.checkerTime;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy = y / (float)gridY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx = x / (float)gridX;

                int cx = (int)(fx * checker_size);
                int cy;
                if (screenDependentRenderMode)
                    cy = (int)(fy * checker_size);
                else
                    cy = (int)(fy * checker_size * aspectY);
                int checker = (cx + cy) % 2;

                float anim_progress;
                if (diagonal_anim)
                {
                    anim_progress = (fx + fy) * 0.5f + time * anim_speed;
                }
                else
                {
                    anim_progress = fx + time * anim_speed;
                }

                anim_progress = fmodf(anim_progress, 2.0f);
                if (anim_progress > 1.0f) anim_progress = 2.0f - anim_progress;
                if (reverse) anim_progress = 1.0f - anim_progress;

                float t;
                if (checker == 0)
                {
                    t = anim_progress - 0.3f;
                }
                else
                {
                    t = anim_progress + 0.3f;
                }

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 10)
    {
        float band = 0.05f + 0.15f * Frand(gen);
        float inv_band = 1.0f / band;
        bool opening = (BdRand() % 2) == 0;
        bool vertical = (BdRand() % 2) == 0;
        float curtain_wrinkles = 0.5f + Frand(gen) * 2.0f;
        float center_gap = 0.05f + Frand(gen) * 0.15f;
        bool reverse_motion = (BdRand() % 2) == 0;

        int repeats = 1 + (BdRand() % 4);
        float repeat_width = 1.0f / repeats;
        float repeat_variation = 0.3f * Frand(gen);
        bool alternate_direction = (BdRand() % 2) == 0;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy = (y / (float)gridY);
            for (int x = 0; x <= gridX; x++)
            {
                float fx = (x / (float)gridX);

                float t;
                if (vertical)
                {
                    float pos = fx;
                    float segment_pos = pos * repeats;
                    int segment_idx = (int)floorf(segment_pos);
                    float segment_local = segment_pos - segment_idx;

                    float center_dist = fabsf(segment_local - 0.5f) - center_gap / 2;
                    if (center_dist < 0) center_dist = 0;

                    float curtain_side = (segment_local < 0.5f) ? -1.0f : 1.0f;

                    t = center_dist * 2.0f;

                    float segment_variation = sinf(segment_idx * 1.618f) * repeat_variation;
                    t += segment_variation;

                    float wrinkles = sinf(fy * 3.14159f * curtain_wrinkles) * 0.1f;
                    t += wrinkles * (1.0f - t);

                    if (opening)
                        t = 1.0f - t;

                    if (alternate_direction && (segment_idx % 2 == 1))
                        curtain_side *= -1.0f;

                    if (reverse_motion)
                        t = curtain_side > 0 ? t : 1.0f - t;
                    else
                        t = curtain_side > 0 ? 1.0f - t : t;
                }
                else
                {
                    float pos = fy;
                    float segment_pos = pos * repeats;
                    int segment_idx = (int)floorf(segment_pos);
                    float segment_local = segment_pos - segment_idx;

                    float center_dist = fabsf(segment_local - 0.5f) - center_gap / 2;
                    if (center_dist < 0) center_dist = 0;

                    float curtain_side = (segment_local < 0.5f) ? -1.0f : 1.0f;

                    t = center_dist * 2.0f;

                    float segment_variation = sinf(segment_idx * 1.618f) * repeat_variation;
                    t += segment_variation;

                    float wrinkles = sinf(fx * 3.14159f * curtain_wrinkles) * 0.1f;
                    t += wrinkles * (1.0f - t);

                    if (opening)
                        t = 1.0f - t;

                    if (alternate_direction && (segment_idx % 2 == 1))
                        curtain_side *= -1.0f;

                    if (reverse_motion)
                        t = curtain_side > 0 ? t : 1.0f - t;
                    else
                        t = curtain_side > 0 ? 1.0f - t : t;
                }

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 11)
    {
        float band = 0.05f + 0.15f * Frand(gen);
        float inv_band = 1.0f / band;
        int bubble_count = 10 + (BdRand() % 30);
        float bubble_size_min = 0.05f + Frand(gen) * 0.1f;
        float bubble_size_max = 0.15f + Frand(gen) * 0.2f;
        bool growing_bubbles = (BdRand() % 2) == 0;

        struct Bubble {
            float x, y;
            float size;
            float speed;
        };

        std::vector<Bubble> bubbles(static_cast<size_t>(bubble_count));
        for (int i = 0; i < bubble_count; i++)
        {
            bubbles[i].x = Frand(gen);
            bubbles[i].y = Frand(gen);
            bubbles[i].size = bubble_size_min + Frand(gen) * (bubble_size_max - bubble_size_min);
            bubbles[i].speed = 0.5f + Frand(gen) * 1.5f;
        }

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy = (y / (float)gridY);
            for (int x = 0; x <= gridX; x++)
            {
                float fx = (x / (float)gridX);

                float max_influence = 0.0f;

                for (int i = 0; i < bubble_count; i++)
                {
                    float dx, dy;
                    if (screenDependentRenderMode)
                    {
                        dx = (fx - bubbles[i].x);
                        dy = (fy - bubbles[i].y);
                    }
                    else
                    {
                        dx = (fx - bubbles[i].x) * aspectX;
                        dy = (fy - bubbles[i].y) * aspectY;
                    }
                    float dist = sqrtf(dx * dx + dy * dy);

                    float influence = 1.0f - (dist / bubbles[i].size);
                    if (influence < 0) influence = 0;

                    influence = influence * influence * (3.0f - 2.0f * influence);

                    if (influence > max_influence)
                        max_influence = influence;
                }

                float t = growing_bubbles ? max_influence : (1.0f - max_influence);

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 12)
    {
        float band = 0.06f + 0.14f * Frand(gen);
        float inv_band = 1.0f / band;

        int segments = 3 + (BdRand() % 9);
        float segment_angle = 6.2831853f / segments;
        float rotation = Frand(gen) * 6.2831853f;
        bool mirror_effect = (BdRand() % 2) == 0;
        float radial_factor = 0.5f + Frand(gen);

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float angle = atan2f(fy, fx) + rotation;
                float radius = sqrtf(fx * fx + fy * fy) * 1.41421356f;

                if (angle < 0) angle += 6.2831853f;
                if (angle >= 6.2831853f) angle -= 6.2831853f;

                int segment = (int)(angle / segment_angle);
                float segment_offset = angle - segment * segment_angle;

                if (mirror_effect && segment_offset > segment_angle * 0.5f) {
                    segment_offset = segment_angle - segment_offset;
                }

                float normalized_angle = segment_offset / segment_angle;

                float t = (normalized_angle * 0.7f + radius * 0.3f * radial_factor);

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 13)
    {
        float band = 0.07f + 0.13f * Frand(gen);
        float inv_band = 1.0f / band;

        float twist_factor = 1.0f + Frand(gen) * 2.0f;
        bool reverse_twist = (BdRand() % 2) == 0;
        float strip_width = 0.3f + Frand(gen) * 0.4f;
        float progress_offset = Frand(gen) * 0.5f;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float angle = atan2f(fy, fx);
                float radius = sqrtf(fx * fx + fy * fy) * 1.41421356f;

                float normalized_angle = (angle + 3.14159265f) / 6.2831853f;

                float twist_progress = (normalized_angle + progress_offset) * twist_factor;
                if (reverse_twist) twist_progress = -twist_progress;

                float moebius_value = radius + 0.3f * sinf(twist_progress * 3.14159265f);

                float t = fmodf(moebius_value * (1.0f / strip_width), 1.0f);

                t = 1.0f - t;

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 14)
    {
        float band = 0.05f + 0.15f * Frand(gen);
        float inv_band = 1.0f / band;
        int points = 5 + (BdRand() % 2);
        float inner_radius = 0.3f + Frand(gen) * 0.4f;
        float rotation = Frand(gen) * 6.2831853f;
        bool reverse = (BdRand() % 2) == 0;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float angle = atan2f(fy, fx) + rotation;
                float radius = sqrtf(fx * fx + fy * fy) * 1.41421356f;

                if (angle < 0) angle += 6.2831853f;
                if (angle >= 6.2831853f) angle -= 6.2831853f;

                float segment = 6.2831853f / points;
                float point_angle = fmodf(angle, segment) / segment;

                float star_radius;
                if (point_angle < 0.5f) {
                    star_radius = inner_radius + (1.0f - inner_radius) * point_angle * 2.0f;
                }
                else {
                    star_radius = 1.0f - (1.0f - inner_radius) * (point_angle - 0.5f) * 2.0f;
                }

                float t = (radius / star_radius);
                if (reverse) t = 1.0f - t;

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 15)
    {
        float band = 0.08f + 0.12f * Frand(gen);
        float inv_band = 1.0f / band;

        int tile_size = 8 + (BdRand() % 25);
        float beat_sync = 0.5f + Frand(gen) * 1.5f;
        bool diagonal_pattern = (BdRand() % 2) == 0;
        bool color_cycling = (BdRand() % 2) == 0;
        float speed_factor = 0.5f + Frand(gen) * 2.0f;

        clock.tileTime += 1 / input.fps;
        float time = clock.tileTime * speed_factor;

        float beat = sinf(time * 3.0f) * 0.5f + 0.5f;
        beat = powf(beat, beat_sync);

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy = y / (float)gridY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx = x / (float)gridX;

                int tile_x = (int)(fx * gridX / tile_size);
                int tile_y = (int)(fy * gridY / tile_size);

                float pattern;
                if (diagonal_pattern)
                {
                    pattern = ((tile_x + tile_y) % 2) * 0.8f + 0.1f;
                }
                else
                {
                    pattern = ((tile_x % 2) == (tile_y % 2)) * 0.8f + 0.1f;
                }

                float anim = sinf(time * 2.0f + tile_x * 0.3f + tile_y * 0.7f) * 0.5f + 0.5f;

                float t = (pattern * 0.7f + anim * 0.3f) * beat;

                if (color_cycling)
                {
                    float hue = fmodf(time * 0.2f + tile_x * 0.1f + tile_y * 0.15f, 1.0f);
                    t = fmodf(t + hue * 0.3f, 1.0f);
                }

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 16)
    {
        float band = 0.08f + 0.04f * Frand(gen);
        float inv_band = 1.0f / band;

        float flame_speed = 0.7f + Frand(gen) * 0.6f;
        float base_height = 0.0f;

        float seed1 = Frand(gen) * 10.0f;
        float seed2 = Frand(gen) * 20.0f;
        float seed3 = Frand(gen) * 30.0f;

        clock.fireTime += 1 / input.fps;
        float time = clock.fireTime;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy = (y / (float)gridY);
            for (int x = 0; x <= gridX; x++)
            {
                float fx = (x / (float)gridX);

                float random_flame =
                    sinf(fx * 15.0f + seed1 + time * 2.0f) * 0.4f +
                    sinf(fx * 30.0f + seed2 + time * 3.7f) * 0.2f +
                    sinf(fx * 45.0f + seed3 + time * 5.3f) * 0.1f;

                float flame_shape = (1.0f - fy) * (0.3f + random_flame * 0.7f);

                float flame_front = fmodf(time * flame_speed, 1.5f);

                float t = 1.0f - (fy - flame_front + flame_shape);

                t = (t < 0) ? 0 : ((t > 1) ? 1 : t);

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 17)
    {
        float band = 0.05f + 0.15f * Frand(gen);
        float inv_band = 1.0f / band;

        float swirl_intensity = 2.0f + Frand(gen) * 3.0f;
        float drain_speed = 0.5f + Frand(gen) * 1.5f;
        bool clockwise = (BdRand() % 2) == 0;
        float center_pull = 0.7f + Frand(gen) * 0.6f;
        bool invert = (BdRand() % 2) == 0;

        clock.drainTime += 1 / input.fps;
        float time = clock.drainTime * drain_speed;

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float radius = sqrtf(fx * fx + fy * fy) * 1.41421356f;
                float angle = atan2f(fy, fx);

                float swirl_factor = (1.0f - radius) * swirl_intensity;
                if (clockwise) swirl_factor = -swirl_factor;

                float swirled_angle = angle + swirl_factor + time * 2.0f;

                float t = radius * center_pull + (1.0f - center_pull) *
                    (0.5f + 0.5f * sinf(swirled_angle * 2.0f + radius * 5.0f));

                if (invert)
                    t = 1.0f - t;

                vertInfo[nVert].a = inv_band * (1.0f + band);
                vertInfo[nVert].c = -inv_band + inv_band * t;
                nVert++;
            }
        }
    }
    else if (mixtype == 18)
    {
        float band = 0.08f + 0.12f * Frand(gen);
        float inv_band = 1.0f / band;

        float julia_real = -0.8f + Frand(gen) * 1.6f;
        float julia_imag = -0.8f + Frand(gen) * 1.6f;
        int max_iterations = 20 + (BdRand() % 20);
        float zoom = 0.7f + Frand(gen) * 1.6f;
        float rotation = Frand(gen) * 6.2831853f;

        const bool smooth_coloring = true;

        float edge_softness = 0.3f + Frand(gen) * 0.5f;
        float contrast = 0.7f + Frand(gen) * 0.6f;

        float cos_rot = cosf(rotation);
        float sin_rot = sinf(rotation);

        float min_val = FLT_MAX;
        float max_val = -FLT_MAX;
        std::vector<float> values((gridY + 1) * (gridX + 1));

        int nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            float fy;
            if (screenDependentRenderMode)
                fy = (y / (float)gridY - 0.5f);
            else
                fy = (y / (float)gridY - 0.5f) * aspectY;

            for (int x = 0; x <= gridX; x++)
            {
                float fx;
                if (screenDependentRenderMode)
                    fx = (x / (float)gridX - 0.5f);
                else
                    fx = (x / (float)gridX - 0.5f) * aspectX;

                float zx = (fx * cos_rot - fy * sin_rot) * zoom;
                float zy = (fx * sin_rot + fy * cos_rot) * zoom;

                float cx = julia_real;
                float cy = julia_imag;
                int i;
                for (i = 0; i < max_iterations; i++)
                {
                    float tmp = zx * zx - zy * zy + cx;
                    zy = 2 * zx * zy + cy;
                    zx = tmp;

                    if (zx * zx + zy * zy > 4.0f)
                        break;
                }

                float t;
                if (i < max_iterations)
                {
                    float log_zn = logf(zx * zx + zy * zy) / 2.0f;
                    float nu = logf(log_zn / logf(2.0f)) / logf(2.0f);
                    t = (i + 1 - nu) / max_iterations;
                }
                else
                {
                    t = 1.0f;
                }

                t = powf(t, contrast);

                values[nVert] = t;
                if (t < min_val) min_val = t;
                if (t > max_val) max_val = t;
                nVert++;
            }
        }

        float range = max_val - min_val;
        if (range < 0.0001f) range = 1.0f;

        nVert = 0;
        for (int y = 0; y <= gridY; y++)
        {
            for (int x = 0; x <= gridX; x++)
            {
                float t = (values[nVert] - min_val) / range;

                t = t * t * (3.0f - 2.0f * t) * (1.0f - edge_softness) + t * edge_softness;

                vertInfo[nVert].a = inv_band * (1.0f + band * 1.5f);
                vertInfo[nVert].c = -inv_band + inv_band * t * 1.1f;

                vertInfo[nVert].c = std::max(-10.0f, std::min(10.0f, vertInfo[nVert].c));
                nVert++;
            }
        }
    }
}

}
