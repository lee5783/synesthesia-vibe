//
//  audio_fft_loudness_test.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-01.
//

#include <gtest/gtest.h>

#include "sound_analysis.h"
#include "analysis_settings.h"
#include "audiobuf.h"

#if defined(_MSC_VER)
#include "wavaudiosource.h"
#include <windows.h>
#else
#include "wav_reader.h"
#endif

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr int kNumWaveformSamples = 480;
    constexpr int kMaxAlignOctaves = 10;
    constexpr int kRingReadSamples = 2304;
    constexpr unsigned kSamplesPerRenderFrame = 735;
    constexpr int kFrameCount = 600;

    constexpr AudioAnalysisSettings kDeterministicAnalysisSettings = {
        0, 250,
        250, 4000,
        4000, 20000,
        0.45f, 0.10f,
        1.0f
    };
    constexpr float kFftAttack = 0.5f;
    constexpr float kFftDecay = 0.7f;

    const char* const kCaptureIds[] = {
        "preset-glowsticks-v2-02",
        "preset-cat-scan",
        "preset-dark-side-of-the-moon",
        "preset-now-and-later",
        "preset-transcendance-remix",
        "preset-fractopia-upspoken-mix",
        "preset-jesters-calling-2",
        "preset-tiger-no-5",
        "preset-fuzzy-sciences",
        "preset-shifter-swarm",
        "blend-catscan-to-fractopia",
        "blend-shifterswarm-to-glowsticks",
        "blend-transcendance-to-tigerno5",
    };

    uint32_t FloatBits(float value)
    {
        uint32_t bits = 0;
        memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    float BitsToFloat(uint32_t bits)
    {
        float value = 0.0f;
        memcpy(&value, &bits, sizeof(value));
        return value;
    }

#if defined(_MSC_VER)
    std::wstring Utf8ToWide(const std::string& narrow)
    {
        if (narrow.empty())
            return std::wstring();
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
        std::wstring result(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), (int)narrow.size(), result.data(), sizeNeeded);
        return result;
    }

    using TestWavSource = WavAudioSource;

    std::wstring TestWavPath() { return Utf8ToWide(SYNESTHESIA_VIBE_PHASE2_WAV_PATH); }
#else
    class TestWavSource
    {
    public:
        bool Load(const std::string& path) { return m_reader.Load(path); }

        unsigned int SampleRate() const { return static_cast<unsigned int>(m_reader.SampleRate()); }

        bool FeedNextRenderFrame(unsigned int framesPerRenderFrame)
        {
            if (!m_reader.FeedNextRenderFrame(framesPerRenderFrame, m_pcm))
                return false;

            const int blockAlign = m_reader.Channels() * (m_reader.BitsPerSample() / 8);
            const AudioCaptureFormat format{
                m_reader.SampleRate(), m_reader.Channels(), m_reader.BitsPerSample(), blockAlign
            };
            SetAudioBuf(reinterpret_cast<const unsigned char*>(m_pcm.data()), framesPerRenderFrame, format, true);
            return true;
        }

    private:
        synesthesia_vibe_demo::WavReader m_reader;
        std::vector<int16_t> m_pcm;
    };

    std::string TestWavPath() { return SYNESTHESIA_VIBE_PHASE2_WAV_PATH; }
#endif

    std::string ResolvePhase2ReferenceDir()
    {
        if (const char* env = std::getenv("SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR"))
            return env;
        return SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR;
    }

    std::vector<uint32_t> LoadFpsReferenceHex()
    {
        std::vector<uint32_t> values;
        std::string path = std::string(SYNESTHESIA_VIBE_RNG_TEST_DATA_DIR) + "/phase2-audio-fps-reference.json";
        std::ifstream file(path);
        EXPECT_TRUE(file.is_open()) << "fps reference file not found at " << path;
        if (!file.is_open())
            return values;

        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        size_t pos = 0;
        while (true)
        {
            pos = contents.find("0x", pos);
            if (pos == std::string::npos)
                break;
            size_t end = pos + 2;
            while (end < contents.size() && isxdigit((unsigned char)contents[end]))
                ++end;
            values.push_back(static_cast<uint32_t>(std::stoul(contents.substr(pos + 2, end - pos - 2), nullptr, 16)));
            pos = end;
        }
        return values;
    }

    std::vector<float> ReadFloatBinFile(const fs::path& path, size_t expectedFloats)
    {
        std::vector<float> data;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return data;
        data.resize(expectedFloats);
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(expectedFloats * sizeof(float)));
        if (static_cast<size_t>(file.gcount()) != expectedFloats * sizeof(float))
            data.clear();
        return data;
    }

    bool ExtractHexField(const std::string& json, const std::string& fieldName, uint32_t& outBits)
    {
        std::string needle = "\"" + fieldName + "\": {\"hex\":\"0x";
        size_t pos = json.find(needle);
        if (pos == std::string::npos)
            return false;
        pos += needle.size();
        size_t end = json.find('"', pos);
        if (end == std::string::npos)
            return false;
        outBits = static_cast<uint32_t>(std::stoul(json.substr(pos, end - pos), nullptr, 16));
        return true;
    }

    struct SpectraReference
    {
        bool valid = false;
        uint32_t bass = 0, mid = 0, treb = 0;
        uint32_t bassAtt = 0, midAtt = 0, trebAtt = 0;
    };

    SpectraReference ReadSpectraJson(const fs::path& path)
    {
        SpectraReference result;
        std::ifstream file(path);
        if (!file.is_open())
            return result;
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        result.valid =
            ExtractHexField(contents, "bass", result.bass) &&
            ExtractHexField(contents, "mid", result.mid) &&
            ExtractHexField(contents, "treb", result.treb) &&
            ExtractHexField(contents, "bass_att", result.bassAtt) &&
            ExtractHexField(contents, "mid_att", result.midAtt) &&
            ExtractHexField(contents, "treb_att", result.trebAtt);
        return result;
    }

    class WaveAligner
    {
    public:
        void Apply(td_soundinfo& sound)
        {
            const int nSamples = kNumWaveformSamples;
            const int octaves = ComputeOctaves(nSamples);
            if (octaves < 4)
                return;

            int alignOffset[2] = { 0, 0 };
            for (int ch = 0; ch < 2; ++ch)
                alignOffset[ch] = ComputeChannelOffset(sound, ch, nSamples, octaves);

            memcpy(m_oldwave[0], sound.fWaveform[0], sizeof(float) * 576);
            memcpy(m_oldwave[1], sound.fWaveform[1], sizeof(float) * 576);
            m_prevAlignOffset[0] = alignOffset[0];
            m_prevAlignOffset[1] = alignOffset[1];

            for (int ch = 0; ch < 2; ++ch)
            {
                if (alignOffset[ch] > 0)
                {
                    for (int i = 0; i < nSamples; ++i)
                        sound.fWaveform[ch][i] = sound.fWaveform[ch][i + alignOffset[ch]];
                    memset(&sound.fWaveform[ch][nSamples], 0, (576 - nSamples) * sizeof(float));
                }
            }
        }

    private:
        static int ComputeOctaves(int nSamples)
        {
            int octaves = static_cast<int>(floorf(logf(static_cast<float>(576 - nSamples)) / logf(2.0f)));
            if (octaves > kMaxAlignOctaves)
                octaves = kMaxAlignOctaves;
            return octaves;
        }

        int ComputeChannelOffset(const td_soundinfo& sound, int ch, int nSamples, int octaves)
        {
            float tempNew[kMaxAlignOctaves][576];
            float tempOld[kMaxAlignOctaves][576];
            int spls[kMaxAlignOctaves];
            int space[kMaxAlignOctaves];

            memcpy(tempNew[0], sound.fWaveform[ch], sizeof(float) * 576);
            memcpy(tempOld[0], &m_oldwave[ch][m_prevAlignOffset[ch]], sizeof(float) * nSamples);
            spls[0] = 576;
            space[0] = 576 - nSamples;

            for (int octave = 1; octave < octaves; ++octave)
            {
                spls[octave] = spls[octave - 1] / 2;
                space[octave] = space[octave - 1] / 2;
                for (int n = 0; n < spls[octave]; ++n)
                {
                    tempNew[octave][n] = 0.5f * (tempNew[octave - 1][n * 2] + tempNew[octave - 1][n * 2 + 1]);
                    tempOld[octave][n] = 0.5f * (tempOld[octave - 1][n * 2] + tempOld[octave - 1][n * 2 + 1]);
                }
            }

            if (!m_weightsReady)
            {
                m_weightsReady = true;
                for (int octave = 0; octave < octaves; ++octave)
                {
                    int compareSamples = spls[octave] - space[octave];
                    for (int n = 0; n < compareSamples; ++n)
                    {
                        float w;
                        if (n < compareSamples / 2)
                            w = n * 2 / static_cast<float>(compareSamples);
                        else
                            w = (compareSamples - 1 - n) * 2 / static_cast<float>(compareSamples);
                        w = (w - 0.8f) * 5.0f + 0.8f;
                        if (w > 1) w = 1;
                        if (w < 0) w = 0;
                        m_tempWeight[octave][n] = w;
                    }

                    int n = 0;
                    while (n < compareSamples && m_tempWeight[octave][n] == 0)
                        ++n;
                    m_firstNonzeroWeight[octave] = n;

                    n = compareSamples - 1;
                    while (n >= 0 && m_tempWeight[octave][n] == 0)
                        --n;
                    m_lastNonzeroWeight[octave] = n;
                }
            }

            int n1 = 0;
            int n2 = space[octaves - 1];
            int alignOffset = 0;
            for (int octave = octaves - 1; octave >= 0; --octave)
            {
                int lowestErrOffset = -1;
                float lowestErrAmount = 0;
                for (int n = n1; n < n2; ++n)
                {
                    float errSum = 0;
                    for (int i = m_firstNonzeroWeight[octave]; i <= m_lastNonzeroWeight[octave]; ++i)
                    {
                        float x = (tempNew[octave][i + n] - tempOld[octave][i]) * m_tempWeight[octave][i];
                        errSum += (x > 0) ? x : -x;
                    }
                    if (lowestErrOffset == -1 || errSum < lowestErrAmount)
                    {
                        lowestErrOffset = n;
                        lowestErrAmount = errSum;
                    }
                }

                if (octave > 0)
                {
                    n1 = lowestErrOffset * 2 - 1;
                    n2 = lowestErrOffset * 2 + 2 + 1;
                    if (n1 < 0) n1 = 0;
                    if (n2 > space[octave - 1]) n2 = space[octave - 1];
                }
                else
                {
                    alignOffset = lowestErrOffset;
                }
            }

            return alignOffset;
        }

        float m_oldwave[2][576] = {};
        int m_prevAlignOffset[2] = { 0, 0 };
        bool m_weightsReady = false;
        float m_tempWeight[kMaxAlignOctaves][576] = {};
        int m_firstNonzeroWeight[kMaxAlignOctaves] = {};
        int m_lastNonzeroWeight[kMaxAlignOctaves] = {};
    };

    class FftLoudnessHarness
    {
    public:
        FftLoudnessHarness()
        {
            memset(&m_sound, 0, sizeof(m_sound));
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 3; ++i)
                    m_sound.infinite_avg[ch][i] = m_sound.avg[ch][i] = m_sound.med_avg[ch][i] = m_sound.long_avg[ch][i] = 1.0f;

            memset(&m_mysound, 0, sizeof(m_mysound));
            memset(m_fftSmoothed, 0, sizeof(m_fftSmoothed));
            memset(m_fftPeak, 0, sizeof(m_fftPeak));
            memset(m_fftPeakHold, 0, sizeof(m_fftPeakHold));

            m_fftObj.Init(576, NUM_FREQUENCIES);
            m_myFft.Init(MY_FFT_WINDOW, MY_FFT_SAMPLES, -1);
            m_myFftShader.Init(MY_FFT_SHADER_WINDOW, MY_FFT_SHADER_SAMPLES, 0);
        }

        void RunFrame(float fps, int frameIndex)
        {
            unsigned char pcmLeft[kRingReadSamples];
            unsigned char pcmRight[kRingReadSamples];
            GetAudioBuf(pcmLeft, pcmRight, kRingReadSamples);

            AnalyzeNewSoundAudio(pcmLeft, pcmRight, fps, m_sound, m_fftObj);
            m_aligner.Apply(m_sound);

            static const float kFrozenBeatDetection[6] = {};
            ComputeCustomSoundAnalysis(
                kDeterministicAnalysisSettings, fps, frameIndex,
                kFftAttack, kFftDecay, false, kFrozenBeatDetection,
                m_sound, m_mysound, m_myFft, m_myFftShader,
                m_fftSmoothed, m_fftPeak, m_fftPeakHold);
        }

        const float* FftSmoothed() const { return m_fftSmoothed; }
        const float* FftPeak() const { return m_fftPeak; }

        void WaveformSamples(float* outLeft, float* outRight) const
        {
            memcpy(outLeft, m_sound.fWaveform[0], sizeof(float) * kNumWaveformSamples);
            memcpy(outRight, m_sound.fWaveform[1], sizeof(float) * kNumWaveformSamples);
        }

        float Bass() const { return m_mysound.imm_rel[0]; }
        float Mid() const { return m_mysound.imm_rel[1]; }
        float Treb() const { return m_mysound.imm_rel[2]; }
        float BassAtt() const { return m_mysound.avg_rel[0]; }
        float MidAtt() const { return m_mysound.avg_rel[1]; }
        float TrebAtt() const { return m_mysound.avg_rel[2]; }

    private:
        td_soundinfo m_sound = {};
        td_mysounddata m_mysound = {};
        FFT m_fftObj;
        FFT m_myFft;
        FFT m_myFftShader;
        float m_fftSmoothed[MY_FFT_SHADER_SAMPLES] = {};
        float m_fftPeak[MY_FFT_SHADER_SAMPLES] = {};
        int m_fftPeakHold[MY_FFT_SHADER_SAMPLES] = {};
        WaveAligner m_aligner;
    };
}

TEST(AudioFftLoudnessTest, MatchesPhase2DumpAudioReferenceAcrossAllCapturesAndFrames)
{
    const std::string referenceDir = ResolvePhase2ReferenceDir();
    if (!fs::exists(referenceDir))
    {
        GTEST_SKIP() << "P2-017 --dump-audio reference data not found at " << referenceDir
                     << " (gitignored, local-only; set SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR to point at tests/golden/phase2-reference "
                        "or an archive re-extraction of it)";
    }

    std::vector<uint32_t> fpsHexValues = LoadFpsReferenceHex();
    ASSERT_EQ(fpsHexValues.size(), static_cast<size_t>(kFrameCount));

    TestWavSource wav;
    ASSERT_TRUE(wav.Load(TestWavPath())) << "failed to load " << SYNESTHESIA_VIBE_PHASE2_WAV_PATH;
    ASSERT_EQ(wav.SampleRate(), 44100u);

    ResetAudioBuf();
    FftLoudnessHarness harness;

    std::vector<std::array<float, MY_FFT_SHADER_SAMPLES>> fftSmoothedByFrame(kFrameCount);
    std::vector<std::array<float, MY_FFT_SHADER_SAMPLES>> fftPeakByFrame(kFrameCount);
    std::vector<std::array<float, kNumWaveformSamples>> waveLeftByFrame(kFrameCount);
    std::vector<std::array<float, kNumWaveformSamples>> waveRightByFrame(kFrameCount);
    std::vector<float> bassByFrame(kFrameCount);
    std::vector<float> midByFrame(kFrameCount);
    std::vector<float> trebByFrame(kFrameCount);
    std::vector<float> bassAttByFrame(kFrameCount);
    std::vector<float> midAttByFrame(kFrameCount);
    std::vector<float> trebAttByFrame(kFrameCount);

    for (int frame = 0; frame < kFrameCount; ++frame)
    {
        ASSERT_TRUE(wav.FeedNextRenderFrame(kSamplesPerRenderFrame)) << "frame " << frame;

        const float fps = BitsToFloat(fpsHexValues[frame]);
        harness.RunFrame(fps, frame);

        memcpy(fftSmoothedByFrame[frame].data(), harness.FftSmoothed(), sizeof(float) * MY_FFT_SHADER_SAMPLES);
        memcpy(fftPeakByFrame[frame].data(), harness.FftPeak(), sizeof(float) * MY_FFT_SHADER_SAMPLES);

        float waveLeft[kNumWaveformSamples];
        float waveRight[kNumWaveformSamples];
        harness.WaveformSamples(waveLeft, waveRight);
        for (int i = 0; i < kNumWaveformSamples; ++i)
        {
            waveLeftByFrame[frame][i] = waveLeft[i] / 128.0f;
            waveRightByFrame[frame][i] = waveRight[i] / 128.0f;
        }

        bassByFrame[frame] = harness.Bass();
        midByFrame[frame] = harness.Mid();
        trebByFrame[frame] = harness.Treb();
        bassAttByFrame[frame] = harness.BassAtt();
        midAttByFrame[frame] = harness.MidAtt();
        trebAttByFrame[frame] = harness.TrebAtt();
    }

    int framesChecked = 0;
    for (const char* captureId : kCaptureIds)
    {
        const fs::path dumpsDir = fs::path(referenceDir) / captureId / "dumps";
        ASSERT_TRUE(fs::exists(dumpsDir)) << "missing dumps dir for " << captureId << " at " << dumpsDir;

        for (int frame = 0; frame < kFrameCount; ++frame)
        {
            char nameBuf[64];

            snprintf(nameBuf, sizeof(nameBuf), "frame_%04d_fft.bin", frame);
            std::vector<float> referenceFft = ReadFloatBinFile(dumpsDir / nameBuf, MY_FFT_SHADER_SAMPLES * 2);
            ASSERT_EQ(referenceFft.size(), static_cast<size_t>(MY_FFT_SHADER_SAMPLES * 2))
                << captureId << " frame " << frame << " fft.bin unreadable/short";
            for (int i = 0; i < MY_FFT_SHADER_SAMPLES; ++i)
            {
                EXPECT_EQ(FloatBits(referenceFft[i]), FloatBits(fftSmoothedByFrame[frame][i]))
                    << captureId << " frame " << frame << " fftSmoothed[" << i << "]";
                EXPECT_EQ(FloatBits(referenceFft[MY_FFT_SHADER_SAMPLES + i]), FloatBits(fftPeakByFrame[frame][i]))
                    << captureId << " frame " << frame << " fftPeak[" << i << "]";
            }

            snprintf(nameBuf, sizeof(nameBuf), "frame_%04d_wave.bin", frame);
            std::vector<float> referenceWave = ReadFloatBinFile(dumpsDir / nameBuf, kNumWaveformSamples * 2);
            ASSERT_EQ(referenceWave.size(), static_cast<size_t>(kNumWaveformSamples * 2))
                << captureId << " frame " << frame << " wave.bin unreadable/short";
            for (int i = 0; i < kNumWaveformSamples; ++i)
            {
                EXPECT_EQ(FloatBits(referenceWave[i]), FloatBits(waveLeftByFrame[frame][i]))
                    << captureId << " frame " << frame << " waveLeft[" << i << "]";
                EXPECT_EQ(FloatBits(referenceWave[kNumWaveformSamples + i]), FloatBits(waveRightByFrame[frame][i]))
                    << captureId << " frame " << frame << " waveRight[" << i << "]";
            }

            snprintf(nameBuf, sizeof(nameBuf), "frame_%04d_spectra.json", frame);
            SpectraReference spectra = ReadSpectraJson(dumpsDir / nameBuf);
            ASSERT_TRUE(spectra.valid) << captureId << " frame " << frame << " spectra.json unreadable";
            EXPECT_EQ(spectra.bass, FloatBits(bassByFrame[frame])) << captureId << " frame " << frame << " bass";
            EXPECT_EQ(spectra.mid, FloatBits(midByFrame[frame])) << captureId << " frame " << frame << " mid";
            EXPECT_EQ(spectra.treb, FloatBits(trebByFrame[frame])) << captureId << " frame " << frame << " treb";
            EXPECT_EQ(spectra.bassAtt, FloatBits(bassAttByFrame[frame])) << captureId << " frame " << frame << " bass_att";
            EXPECT_EQ(spectra.midAtt, FloatBits(midAttByFrame[frame])) << captureId << " frame " << frame << " mid_att";
            EXPECT_EQ(spectra.trebAtt, FloatBits(trebAttByFrame[frame])) << captureId << " frame " << frame << " treb_att";

            ++framesChecked;
        }
    }

    EXPECT_EQ(framesChecked, kFrameCount * static_cast<int>(sizeof(kCaptureIds) / sizeof(kCaptureIds[0])));
}
