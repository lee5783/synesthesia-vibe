// audiobuf.cpp

#include "audiobuf.h"

#include <cstdint>
#include <cstring>

#define SAMPLE_SIZE_LPB 2304 // Max number of audio samples stored in circular buffer. Larger buffer enables better low-frequency FFT resolution (~5.4 Hz/bin). Should be no less than SAMPLE_SIZE. Expected sampling rate is 44100 Hz or 48000 Hz (samples per second).

float g_fAudioSensitivity = 1.0f;

static bool g_bCaptureMic = false;
static AudioDrainClockFn g_audioDrainClockFn = nullptr;

bool GetCaptureMicFlag() {
    return g_bCaptureMic;
}

void SetCaptureMicFlag(bool bCaptureMic) {
    g_bCaptureMic = bCaptureMic;
}

void SetAudioDrainClock(AudioDrainClockFn clockFn) {
    g_audioDrainClockFn = clockFn;
}

std::mutex pcmLpbMutex;
unsigned char pcmLeftLpb[SAMPLE_SIZE_LPB]; // Circular buffer (left channel)
unsigned char pcmRightLpb[SAMPLE_SIZE_LPB]; // Circular buffer (right channel)
float pcmLeftFloatLpb[SAMPLE_SIZE_LPB];
float pcmRightFloatLpb[SAMPLE_SIZE_LPB];
signed int pcmLen = 0; // Actual number of samples the buffer holds. Can be less than SAMPLE_SIZE_LPB
signed int pcmPos = 0; // Next position written by the circular buffer.
uint64_t pcmLastPacketTick = 0;
uint64_t pcmPacketIntervalMs = 0;
uint32_t pcmSampleRate = 0;

// This is called while pcmLpbMutex is held.  The expiry interval comes from
// the captured packet and the amount of audio retained in the ring, so it
// does not assume a device latency or a render-frame rate.
void DrainAudioBufIfExpired() {
    if (pcmLen == 0 || pcmLastPacketTick == 0 || pcmPacketIntervalMs == 0 ||
        pcmSampleRate == 0 || !g_audioDrainClockFn)
        return;

    // Let the already-captured audio play out before declaring the source
    // stale.  This absorbs normal packet-delivery jitter without retaining a
    // frozen waveform indefinitely after capture stops.
    const uint64_t retainedAudioMs =
        (static_cast<uint64_t>(pcmLen) * 1000ULL) / pcmSampleRate;
    const uint64_t elapsedMs = g_audioDrainClockFn() - pcmLastPacketTick;
    if (elapsedMs > pcmPacketIntervalMs + retainedAudioMs) {
        pcmLen = 0;
    }
}

void ResetAudioBuf() {
    std::unique_lock<std::mutex> lock(pcmLpbMutex);
    memset(pcmLeftLpb, 0, SAMPLE_SIZE_LPB);
    memset(pcmRightLpb, 0, SAMPLE_SIZE_LPB);
    memset(pcmLeftFloatLpb, 0, sizeof(pcmLeftFloatLpb));
    memset(pcmRightFloatLpb, 0, sizeof(pcmRightFloatLpb));
    pcmLen = 0;
    pcmPos = 0;
    pcmLastPacketTick = 0;
    pcmPacketIntervalMs = 0;
    pcmSampleRate = 0;
}

void GetAudioBuf(unsigned char *pWaveL, unsigned char *pWaveR, int SamplesCount) {
    std::unique_lock<std::mutex> lock(pcmLpbMutex);
    DrainAudioBufIfExpired();

    if (pcmLen < SamplesCount) {
        // Buffer underrun. Insufficient new samples in circular buffer (pcmLeftLpb, pcmRightLpb)
        memset(pWaveL, 0, SamplesCount);
        memset(pWaveR, 0, SamplesCount);
    }
    else {
        // Read the latest complete block, independent of packet size or the
        // interval between packet deliveries.
        const int first = (pcmPos - SamplesCount + SAMPLE_SIZE_LPB) % SAMPLE_SIZE_LPB;
        for (int i = 0; i < SamplesCount; i++) {
            // int8_t [-128 .. +127] stored into uint8_t [0..255]
            pWaveL[i] = pcmLeftLpb[(first + i) % SAMPLE_SIZE_LPB];
            pWaveR[i] = pcmRightLpb[(first + i) % SAMPLE_SIZE_LPB];
        }
    }
}

void GetAudioBufFloat(float* pWaveL, float* pWaveR, int SamplesCount) {
    std::unique_lock<std::mutex> lock(pcmLpbMutex);
    DrainAudioBufIfExpired();
    if (pcmLen == 0) {
        memset(pWaveL, 0, SamplesCount * sizeof(float));
        memset(pWaveR, 0, SamplesCount * sizeof(float));
    }
    else {
        // Zero-fill beginning if buffer not yet full; valid audio at end.
        // This allows larger FFT windows to work during startup (zero-padded front
        // is naturally tapered by the FFT window function).
        int available = (pcmLen < SamplesCount) ? pcmLen : SamplesCount;
        int zeroPrefix = SamplesCount - available;
        if (zeroPrefix > 0) {
            memset(pWaveL, 0, zeroPrefix * sizeof(float));
            memset(pWaveR, 0, zeroPrefix * sizeof(float));
        }
        const int first = (pcmPos - available + SAMPLE_SIZE_LPB) % SAMPLE_SIZE_LPB;
        for (int i = 0; i < available; i++) {
            // Match the legacy waveform amplitude domain used by the FFT path:
            // old m_sound.fWaveform samples were roughly in [-128..127].
            pWaveL[zeroPrefix + i] = pcmLeftFloatLpb[(first + i) % SAMPLE_SIZE_LPB] * 128.0f;
            pWaveR[zeroPrefix + i] = pcmRightFloatLpb[(first + i) % SAMPLE_SIZE_LPB] * 128.0f;
        }
    }
}

int8_t FltToInt(float flt) {
    if (flt >= 1.0f) {
        return +127; // 0x7f
    }
    if (flt < -1.0f) {
        return -128; // 0x80
    }
    return (int8_t)(flt * 128);
};

// Union type for sample conversion
union u_type
{
    int32_t IntVar;
    float FltVar;
    uint8_t Bytes[4];
};

int8_t GetChannelSample(const uint8_t *pData, int BlockOffset, int ChannelOffset, const bool bInt16) {
    u_type sample;

    sample.IntVar = 0;
    sample.Bytes[0] = pData[BlockOffset + ChannelOffset + 0];
    sample.Bytes[1] = pData[BlockOffset + ChannelOffset + 1];
    if (!bInt16) {
        sample.Bytes[2] = pData[BlockOffset + ChannelOffset + 2];
        sample.Bytes[3] = pData[BlockOffset + ChannelOffset + 3];
    }

    if (!bInt16) {
        return FltToInt(sample.FltVar); //float [-1.0f .. +1.0f] range converted to int8_t [-128 .. +127] and later stored into uint8_t [0 .. 255]
    }
    else {
        float flt = ((int16_t)sample.IntVar) / 32768.0f;
        return FltToInt(flt); //int16_t [-32768 .. +32767] range converted to int8_t [-128 .. +127] and later stored into uint8_t [0..255]
    }
}

float GetChannelSampleFloat(const uint8_t* pData, int BlockOffset, int ChannelOffset, const bool bInt16) {
    u_type sample;

    sample.IntVar = 0;
    sample.Bytes[0] = pData[BlockOffset + ChannelOffset + 0];
    sample.Bytes[1] = pData[BlockOffset + ChannelOffset + 1];
    if (!bInt16) {
        sample.Bytes[2] = pData[BlockOffset + ChannelOffset + 2];
        sample.Bytes[3] = pData[BlockOffset + ChannelOffset + 3];
        if (sample.FltVar >= 1.0f) return 1.0f;
        if (sample.FltVar <= -1.0f) return -1.0f;
        return sample.FltVar;
    }

    float v = (float)((int16_t)sample.IntVar) / 32768.0f;
    if (v >= 1.0f) return 1.0f;
    if (v <= -1.0f) return -1.0f;
    return v;
}

// Expecting pData holds:
//   signed 16-bit (2 bytes) PCM, Little Endian
//   or
//   32-bit float (4 bytes) PCM
// Supported audio formats:
//   format.channels;          /* ANY number of channels (i.e. mono, stereo...) */
//   format.sampleRate;        /* 44100 or 48000 sample rate */
//   format.blockAlign;        /* ANY block size of data */
//   format.bitsPerSample;     /* 16 or 32 number of bits per sample of mono data */

void SetAudioBuf(const unsigned char *pData, const uint32_t nNumFramesToRead, const AudioCaptureFormat &format, const bool bInt16) {
    std::unique_lock<std::mutex> lock(pcmLpbMutex);
    //memset(pcmLeftLpb, 0, SAMPLE_SIZE_LPB);
    //memset(pcmRightLpb, 0, SAMPLE_SIZE_LPB);

    // Determine downsampling ratio (supporting 96kHz and 192kHz)
    int downsampleRatio = 1;
    if (format.sampleRate > TARGET_SAMPLE_RATE) {
        downsampleRatio = format.sampleRate / TARGET_SAMPLE_RATE;
    }

    // Retain exactly the newest buffer-sized suffix of the packet. This is
    // naturally independent of the host's latency and packet size.
    const uint32_t bufferCapacity = static_cast<uint32_t>(SAMPLE_SIZE_LPB);
    const uint32_t outputSamples = nNumFramesToRead / downsampleRatio;
    const uint32_t firstOutputSample = outputSamples > bufferCapacity
        ? outputSamples - bufferCapacity
        : 0;

    for (uint32_t i = firstOutputSample; i < outputSamples; ++i) {
        float sumLeft = 0.0f;
        float sumRight = 0.0f;

        // Average samples for downsampling
        for (int j = 0; j < downsampleRatio; j++) {
            int inputIndex = i * downsampleRatio + j;

            int blockOffset = inputIndex * format.blockAlign;

            // Get left channel sample
            float sampleLeft = 0.0f;
            if (pData && format.channels >= 1) {
                sampleLeft = GetChannelSampleFloat(pData, blockOffset, 0, bInt16);
            }
            sumLeft += sampleLeft;

            // Get right channel sample (use left if mono)
            float sampleRight = sampleLeft;
            if (pData && format.channels >= 2) {
                sampleRight = GetChannelSampleFloat(pData, blockOffset, format.bitsPerSample / 8, bInt16);
            }
            sumRight += sampleRight;
        }

		/*
        Added support for 96000 Hz and 192000 Hz sample rates.
        Technically downsamples when it's above the target sample rate
        provided in audiobuf.h
        */

        // Ignoring data in all other audio channels (Quadraphonic 4.0, Surround 4.0, Surround 5.1, Surround 7.1, ...)

        // Saving audio data for visualizer
        // 8-bit signed integer in Two's Complement Representation stored in unsigned char array
        // int8_t[-128 .. + 127] stored into uint8_t[0 .. 255]

        float finalLeft = (sumLeft / downsampleRatio) * g_fAudioSensitivity;
        float finalRight = (sumRight / downsampleRatio) * g_fAudioSensitivity;

        // Store averaged/downsampled values
        pcmLeftFloatLpb[pcmPos] = finalLeft;
        pcmRightFloatLpb[pcmPos] = finalRight;
        pcmLeftLpb[pcmPos] = (uint8_t)FltToInt(finalLeft);
        pcmRightLpb[pcmPos] = (uint8_t)FltToInt(finalRight);

        pcmPos = (pcmPos + 1) % SAMPLE_SIZE_LPB;
        if (pcmLen < SAMPLE_SIZE_LPB)
            ++pcmLen;
    }

    if (outputSamples != 0 && format.sampleRate != 0 && g_audioDrainClockFn) {
        const uint64_t now = g_audioDrainClockFn();
        // Convert the actual capture-frame duration to milliseconds.  This is
        // a unit conversion, not a fixed timeout.
        uint64_t intervalMs =
            (static_cast<uint64_t>(nNumFramesToRead) * 1000ULL) /
            format.sampleRate;

        // On hosts that deliver packets less frequently than their nominal
        // duration, retain samples until the observed delivery interval.
        // This keeps high-latency devices from incorrectly going flatline.
        if (pcmLastPacketTick != 0) {
            const uint64_t observedIntervalMs = now - pcmLastPacketTick;
            if (observedIntervalMs > intervalMs)
                intervalMs = observedIntervalMs;
        }

        pcmLastPacketTick = now;
        pcmPacketIntervalMs = intervalMs;
        pcmSampleRate = format.sampleRate / downsampleRatio;
    }

}
