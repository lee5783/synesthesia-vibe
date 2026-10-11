//
//  noise_textures.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "noise_textures.h"
#include "gl_error.h"
#include "gl_object_counts.h"
#include "gl_headers.h"
#include "noise.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace render_gl
{

namespace
{

struct NoiseSpec
{
    const char* szName;
    int size;
    int zoomFactor;
};

const NoiseSpec kNoiseSpecs[kNoiseTextureCount] = {
    { "noise_lq",      256, 1 },
    { "noise_lq_lite",  32, 1 },
    { "noise_mq",      256, 4 },
    { "noise_hq",      256, 8 },
    { "noisevol_lq",    32, 1 },
    { "noisevol_hq",    32, 4 },
};

#if defined(SYNESTHESIA_VIBE_GLES)
constexpr unsigned int kUploadFormat = GL_RGBA;
#else
constexpr unsigned int kUploadFormat = GL_BGRA;
#endif

size_t PixelCount(int index)
{
    const size_t size = static_cast<size_t>(kNoiseSpecs[index].size);
    return index >= kNoiseTexture2DCount ? size * size * size : size * size;
}

#if defined(SYNESTHESIA_VIBE_GLES)
void SwizzleBgraToRgba(std::vector<uint32_t>& pixels)
{
    for (uint32_t& pixel : pixels)
        pixel = (pixel & 0xFF00FF00u) | ((pixel & 0x00FF0000u) >> 16) | ((pixel & 0x000000FFu) << 16);
}
#else
void WriteDump(const char* szDumpDir, const char* szName, const std::vector<uint32_t>& pixels)
{
    if (szDumpDir == nullptr || szDumpDir[0] == '\0')
        return;

    const std::string path = std::string(szDumpDir) + "/" + szName + ".bgra";
    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr)
        return;

    fwrite(pixels.data(), sizeof(uint32_t), pixels.size(), file);
    fclose(file);
}

int VerifyLevel0(int index, unsigned int target, const std::vector<uint32_t>& expected, const char* szDumpDir)
{
    std::vector<uint32_t> readback(expected.size(), 0u);
    glGetTexImage(target, 0, GL_BGRA, GL_UNSIGNED_BYTE, readback.data());

    int mismatches = 0;
    for (size_t i = 0; i < expected.size(); i++)
    {
        if (readback[i] != expected[i])
            mismatches++;
    }

    WriteDump(szDumpDir, kNoiseSpecs[index].szName, readback);

    fprintf(stderr, "render_gl: noise verify %s size=%d texels=%zu mismatches=%d\n",
        kNoiseSpecs[index].szName, kNoiseSpecs[index].size, expected.size(), mismatches);

    return mismatches;
}
#endif

}

NoiseTextures::NoiseTextures()
    : m_verifyMismatches(0)
{
    memset(m_ids, 0, sizeof(m_ids));
}

NoiseTextures::~NoiseTextures()
{
    Destroy();
}

int NoiseTextures::Size(int index) const
{
    return kNoiseSpecs[index].size;
}

const char* NoiseTextures::Name(int index) const
{
    return kNoiseSpecs[index].szName;
}

int NoiseTextures::FindByName(const char* szName) const
{
    for (int i = 0; i < kNoiseTextureCount; i++)
    {
        if (strcmp(kNoiseSpecs[i].szName, szName) == 0)
            return i;
    }

    return -1;
}

bool NoiseTextures::Create(std::mt19937_64& gen, render_core::IRawClock& clock, const NoiseVerifyOptions& verify)
{
    Destroy();

    for (int i = 0; i < kNoiseTextureCount; i++)
    {
        const NoiseSpec& spec = kNoiseSpecs[i];
        const bool b3D = Is3D(i);
        const unsigned int target = b3D ? GL_TEXTURE_3D : GL_TEXTURE_2D;

        std::vector<uint32_t> pixels(PixelCount(i));
        if (b3D)
            render_core::GenerateNoiseVol(pixels.data(), spec.size, spec.zoomFactor, gen, clock);
        else
            render_core::GenerateNoiseTex(pixels.data(), spec.size, spec.zoomFactor, gen, clock);

        std::vector<uint32_t> uploaded(pixels);
#if defined(SYNESTHESIA_VIBE_GLES)
        SwizzleBgraToRgba(uploaded);
#endif

        glGenTextures(1, &m_ids[i]);
        NoteGlObjectCreated(GlObjectKind::Texture);

        if (!CheckGlError("NoiseTextures::Create glGenTextures"))
        {
            Destroy();
            return false;
        }

        glBindTexture(target, m_ids[i]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        if (b3D)
            glTexImage3D(target, 0, GL_RGBA8, spec.size, spec.size, spec.size, 0, kUploadFormat, GL_UNSIGNED_BYTE, uploaded.data());
        else
            glTexImage2D(target, 0, GL_RGBA8, spec.size, spec.size, 0, kUploadFormat, GL_UNSIGNED_BYTE, uploaded.data());

        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        if (b3D)
            glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_REPEAT);

        glGenerateMipmap(target);

        if (!CheckGlError("NoiseTextures::Create upload"))
        {
            Destroy();
            return false;
        }

        if (verify.enabled)
        {
#if defined(SYNESTHESIA_VIBE_GLES)
            fprintf(stderr, "render_gl: noise verify %s unsupported on ES\n", spec.szName);
#else
            m_verifyMismatches += VerifyLevel0(i, target, pixels, verify.szDumpDir);
#endif
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);

    return true;
}

void NoiseTextures::Destroy()
{
    for (int i = 0; i < kNoiseTextureCount; i++)
    {
        if (m_ids[i] != 0)
        {
            glDeleteTextures(1, &m_ids[i]);
            NoteGlObjectDeleted(GlObjectKind::Texture);
            m_ids[i] = 0;
        }
    }

    m_verifyMismatches = 0;
}

}
