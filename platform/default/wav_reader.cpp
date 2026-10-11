//
//  wav_reader.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "wav_reader.h"

#include <cstring>
#include <fstream>

namespace
{
    struct RiffChunkHeader
    {
        char id[4];
        uint32_t size;
    };

    struct WavFormat
    {
        uint16_t audioFormat;
        uint16_t channels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
    };

    constexpr uint16_t kPcmAudioFormat = 1;
    constexpr uint16_t kRequiredBitsPerSample = 16;
}

namespace synesthesia_vibe_demo
{

bool WavReader::Load(const std::string& path)
{
    m_samples.clear();
    m_next_frame_offset = 0;
    m_frame_count = 0;
    m_channels = 0;
    m_bits_per_sample = 0;
    m_sample_rate = 0;

    std::ifstream file(path, std::ios::binary);

    if (!file.is_open())
        return false;

    char riffId[4] = {};

    file.read(riffId, sizeof(riffId));

    if (file.gcount() != static_cast<std::streamsize>(sizeof(riffId)) || std::memcmp(riffId, "RIFF", 4) != 0)
        return false;

    uint32_t riffSize = 0;
    file.read(reinterpret_cast<char*>(&riffSize), sizeof(riffSize));

    char waveId[4] = {};
    file.read(waveId, sizeof(waveId));

    if (std::memcmp(waveId, "WAVE", 4) != 0)
        return false;

    WavFormat format = {};
    std::vector<unsigned char> pcmBytes;
    bool haveFormat = false;
    bool haveData = false;

    while (file.good() && !(haveFormat && haveData))
    {
        RiffChunkHeader chunk = {};
        file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk));

        if (file.gcount() != static_cast<std::streamsize>(sizeof(chunk)))
            break;

        if (std::memcmp(chunk.id, "fmt ", 4) == 0)
        {
            std::vector<unsigned char> fmtBytes(chunk.size);
            file.read(reinterpret_cast<char*>(fmtBytes.data()), chunk.size);
            std::memcpy(&format, fmtBytes.data(), (std::min)(sizeof(format), fmtBytes.size()));
            haveFormat = true;
        }
        else if (std::memcmp(chunk.id, "data", 4) == 0)
        {
            pcmBytes.resize(chunk.size);
            file.read(reinterpret_cast<char*>(pcmBytes.data()), chunk.size);
            haveData = true;
        }
        else
        {
            file.seekg(chunk.size, std::ios::cur);
        }

        if ((chunk.size % 2) != 0)
            file.seekg(1, std::ios::cur);
    }

    if (!haveFormat || !haveData)
        return false;

    if (format.audioFormat != kPcmAudioFormat || format.bitsPerSample != kRequiredBitsPerSample)
        return false;

    if (format.channels == 0 || format.blockAlign == 0)
        return false;

    m_channels = static_cast<int>(format.channels);
    m_bits_per_sample = static_cast<int>(format.bitsPerSample);
    m_sample_rate = static_cast<int>(format.sampleRate);

    m_samples.resize(pcmBytes.size() / sizeof(int16_t));

    if (!pcmBytes.empty() && !m_samples.empty())
        std::memcpy(m_samples.data(), pcmBytes.data(), m_samples.size() * sizeof(int16_t));

    m_frame_count = m_samples.size() / static_cast<size_t>(m_channels);

    return m_frame_count > 0;
}

bool WavReader::FeedNextRenderFrame(size_t framesPerRenderFrame, std::vector<int16_t>& outInterleaved)
{
    if (m_frame_count == 0 || m_channels == 0)
        return false;

    if (m_next_frame_offset + framesPerRenderFrame > m_frame_count)
        m_next_frame_offset = 0;

    const size_t sampleCount = framesPerRenderFrame * static_cast<size_t>(m_channels);
    outInterleaved.resize(sampleCount);

    if (sampleCount != 0)
    {
        std::memcpy(outInterleaved.data(),
            m_samples.data() + (m_next_frame_offset * static_cast<size_t>(m_channels)),
            sampleCount * sizeof(int16_t));
    }

    m_next_frame_offset += framesPerRenderFrame;
    return true;
}

}