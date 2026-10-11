//
//  sampler_resolver.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-06.
//

#ifndef __BEATDROP_RENDER_CORE_SAMPLER_RESOLVER_H__
#define __BEATDROP_RENDER_CORE_SAMPLER_RESOLVER_H__ 1

#include "shader_constants.h"

namespace render_core
{

enum class TexCode
{
    Disk = 0,
    Vs = 1,
    Fft = 2,
    Wave = 3,
    Blur0 = 4,
    Blur1 = 5,
    Blur2 = 6,
    Blur3 = 7,
    Blur4 = 8,
    Blur5 = 9,
    Blur6 = 10,
    BlurLast = 11
};

constexpr int kSamplerRootNameMax = 260;
constexpr int kMaxRandomTextureSlot = 100;
constexpr int kConstVectorHandleCount = 24;

class IRandomTextureSource
{
public:
    virtual ~IRandomTextureSource() {}
    virtual bool PickRandomTexture(const char* szPrefix, char* szRetTextureFilename, int nMaxBytes) = 0;
};

struct RandomTextureSlots
{
    char names[kMaxRandomTextureSlot][kSamplerRootNameMax];
};

struct SamplerResolution
{
    char rootName[kSamplerRootNameMax];
    TexCode texCode;
    bool bWrap;
    bool bBilinear;
};

void ResolveSamplerName(const char* szConstantName, int nBlurTexCount,
                        IRandomTextureSource* pRandomTextures, RandomTextureSlots& randSlots,
                        SamplerResolution& out);

enum class Float4ConstantKind
{
    None = 0,
    RotMatrix,
    RandFrame,
    RandPreset,
    TexSize,
    ConstVector,
    QConst
};

struct Float4ConstantResolution
{
    Float4ConstantKind kind;
    int index;
    char rootName[kSamplerRootNameMax];
};

Float4ConstantResolution ResolveFloat4Constant(const char* szConstantName, bool bMatrixColumns, bool bVector,
                                               const RandomTextureSlots& randSlots);

}

#endif