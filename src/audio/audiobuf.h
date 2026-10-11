// audiobuf.h

#include <cstdint>
#include <mutex>

#define TARGET_SAMPLE_RATE 44100

extern float g_fAudioSensitivity;

bool GetCaptureMicFlag();
void SetCaptureMicFlag(bool bCaptureMic);

// Reset audio buffer discarding stored audio data
void ResetAudioBuf();

// Return previously saved audio data for visualizer
void GetAudioBuf(unsigned char *pWaveL, unsigned char *pWaveR, int SamplesCount);

// Return previously saved audio data as normalized float samples in [-1..1]
void GetAudioBufFloat(float* pWaveL, float* pWaveR, int SamplesCount);

struct AudioCaptureFormat {
    int sampleRate;
    int channels;
    int bitsPerSample;
    int blockAlign;
};

// Save audio data for visualizer
void SetAudioBuf(const unsigned char *pData, const uint32_t nNumFramesToRead, const AudioCaptureFormat &format, const bool bInt16);

typedef uint64_t (*AudioDrainClockFn)();
void SetAudioDrainClock(AudioDrainClockFn clockFn);
