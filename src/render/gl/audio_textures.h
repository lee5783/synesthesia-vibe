//
//  audio_textures.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_AUDIO_TEXTURES_H__
#define __BEATDROP_RENDER_GL_AUDIO_TEXTURES_H__ 1

namespace render_gl
{

constexpr int kFftTextureSamples = 1024;
constexpr int kWaveTextureSamples = 480;

class AudioTextures
{
public:
    AudioTextures();
    ~AudioTextures();
    AudioTextures(const AudioTextures&) = delete;
    AudioTextures& operator=(const AudioTextures&) = delete;

    bool Create();
    void Destroy();
    bool Valid() const { return m_fft != 0 && m_wave != 0; }
    void Upload(const float* fftSmoothed, const float* fftPeak, const float* waveLeft, const float* waveRight);
    unsigned int FftId() const { return m_fft; }
    unsigned int WaveId() const { return m_wave; }

private:
    unsigned int m_fft;
    unsigned int m_wave;
};

}

#endif
