//
//  wav_reader.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#ifndef __SYNESTHESIA_VIBE_DEMO_SHARED_WAV_READER_H__
#define __SYNESTHESIA_VIBE_DEMO_SHARED_WAV_READER_H__ 1

#include <cstdint>
#include <string>
#include <vector>

namespace synesthesia_vibe_demo
{

class WavReader
{
public:
    bool Load(const std::string& path);
    bool FeedNextRenderFrame(size_t framesPerRenderFrame, std::vector<int16_t>& outInterleaved);

    int Channels() const { return m_channels; }
    int BitsPerSample() const { return m_bits_per_sample; }
    int SampleRate() const { return m_sample_rate; }
    size_t FrameCount() const { return m_frame_count; }

private:
    std::vector<int16_t> m_samples;
    size_t m_next_frame_offset = 0;
    size_t m_frame_count = 0;
    int m_channels = 0;
    int m_bits_per_sample = 0;
    int m_sample_rate = 0;
};

}

#endif