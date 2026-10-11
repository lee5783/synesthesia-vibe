//
//  mesh.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-02.
//

#ifndef __BEATDROP_RENDER_CORE_MESH_H__
#define __BEATDROP_RENDER_CORE_MESH_H__ 1

#include <cstdint>

class CState;

namespace render_core
{

struct Vertex
{
    float x, y, z;
    uint32_t Diffuse;
    float tu, tv;
    float tu_orig, tv_orig;
    float rad, ang;
};

struct VertInfo
{
    float rad;
    float ang;
    float a;
    float c;
};

float SquishToCenter(float x, float fExp);

int GetNearestPow2Size(int w, int h);

void ComputeAspectRatios(int texSizeX, int texSizeY, float* aspectX, float* aspectY, float* invAspectX, float* invAspectY);

void UvToMathSpace(bool screenDependentRenderMode, float aspectX, float aspectY, float u, float v, float* rad, float* ang);

void BuildCompositeMesh(int gridX, int gridY, int viewportWidth, int viewportHeight,
                         bool screenDependentRenderMode, float aspectX, float aspectY,
                         Vertex* outVerts, int* outIndices);

void GetSafeBlurMinMax(CState* pState, float* blur_min, float* blur_max);

struct GridAlphaInput
{
    CState* state;
    CState* oldState;
    float time;
    bool screenDependentRenderMode;
    float aspectX;
    float aspectY;
    float invAspectX;
    float invAspectY;
    int texSizeX;
    int texSizeY;
    int gridX;
    int gridY;
};

void ComputeGridAlphaValues(const GridAlphaInput& input, Vertex* verts, const VertInfo* vertinfo);

}

#endif
