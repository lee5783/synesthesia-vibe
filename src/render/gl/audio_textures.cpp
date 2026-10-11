//
//  audio_textures.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "audio_textures.h"
#include "gl_error.h"
#include "gl_object_counts.h"
#include "gl_headers.h"

#include <cstdio>
#include <vector>

namespace render_gl
{

namespace
{

#if defined(SYNESTHESIA_VIBE_GLES)
int ChooseRowInternalFormat()
{
    const bool bFloatLinear = GLAD_GL_OES_texture_float_linear != 0;
    const char* szRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    fprintf(stderr, "render_gl: audio textures %s (GL_OES_texture_float_linear %s) GL_RENDERER \"%s\"\n",
        bFloatLinear ? "R32F" : "R16F", bFloatLinear ? "present" : "absent", szRenderer != nullptr ? szRenderer : "(null)");

    return bFloatLinear ? GL_R32F : GL_R16F;
}
#else
int ChooseRowInternalFormat()
{
    return GL_R32F;
}
#endif

bool CreateRowTexture(unsigned int& id, int width, int internalFormat)
{
    glGenTextures(1, &id);
    NoteGlObjectCreated(GlObjectKind::Texture);

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, 2, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return CheckGlError("AudioTextures::Create");
}

void UploadRows(unsigned int id, int width, const std::vector<float>& rows)
{
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, 2, GL_RED, GL_FLOAT, rows.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

}

AudioTextures::AudioTextures()
    : m_fft(0)
    , m_wave(0)
{
}

AudioTextures::~AudioTextures()
{
    Destroy();
}

bool AudioTextures::Create()
{
    Destroy();

    const int internalFormat = ChooseRowInternalFormat();

    if (!CreateRowTexture(m_fft, kFftTextureSamples, internalFormat) || !CreateRowTexture(m_wave, kWaveTextureSamples, internalFormat))
    {
        Destroy();
        return false;
    }

    return true;
}

void AudioTextures::Destroy()
{
    unsigned int* ids[2] = { &m_fft, &m_wave };

    for (unsigned int* id : ids)
    {
        if (*id != 0)
        {
            glDeleteTextures(1, id);
            NoteGlObjectDeleted(GlObjectKind::Texture);
            *id = 0;
        }
    }
}

void AudioTextures::Upload(const float* fftSmoothed, const float* fftPeak, const float* waveLeft, const float* waveRight)
{
    if (!Valid())
        return;

    std::vector<float> fftRows(static_cast<size_t>(kFftTextureSamples) * 2);

    for (int i = 0; i < kFftTextureSamples; i++)
    {
        fftRows[i] = fftSmoothed[i];
        fftRows[kFftTextureSamples + i] = fftPeak[i];
    }

    UploadRows(m_fft, kFftTextureSamples, fftRows);

    std::vector<float> waveRows(static_cast<size_t>(kWaveTextureSamples) * 2);

    for (int i = 0; i < kWaveTextureSamples; i++)
    {
        waveRows[i] = waveLeft[i] / 128.0f;
        waveRows[kWaveTextureSamples + i] = waveRight[i] / 128.0f;
    }

    UploadRows(m_wave, kWaveTextureSamples, waveRows);
}

}
