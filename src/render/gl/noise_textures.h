//
//  noise_textures.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_NOISE_TEXTURES_H__
#define __BEATDROP_RENDER_GL_NOISE_TEXTURES_H__ 1

#include <random>

#include "time_model.h"

namespace render_gl
{

constexpr int kNoiseTexture2DCount = 4;
constexpr int kNoiseTexture3DCount = 2;
constexpr int kNoiseTextureCount = kNoiseTexture2DCount + kNoiseTexture3DCount;

struct NoiseVerifyOptions
{
    bool enabled;
    const char* szDumpDir;
};

class NoiseTextures
{
public:
    NoiseTextures();
    ~NoiseTextures();

    NoiseTextures(const NoiseTextures&) = delete;
    NoiseTextures& operator=(const NoiseTextures&) = delete;

    bool Create(std::mt19937_64& gen, render_core::IRawClock& clock, const NoiseVerifyOptions& verify);
    void Destroy();

    bool Valid() const { return m_ids[0] != 0; }

    unsigned int Id(int index) const { return m_ids[index]; }
    bool Is3D(int index) const { return index >= kNoiseTexture2DCount; }
    int Size(int index) const;
    const char* Name(int index) const;
    int FindByName(const char* szName) const;

    int VerifyMismatchCount() const { return m_verifyMismatches; }

private:
    unsigned int m_ids[kNoiseTextureCount];
    int m_verifyMismatches;
};

}

#endif
