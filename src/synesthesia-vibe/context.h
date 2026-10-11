//
//  context.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_CONTEXT_H__
#define __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_CONTEXT_H__ 1

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "synesthesia-vibe/synesthesia-vibe.h"

#include "analysis_settings.h"
#include "audiobuf.h"
#include "fft.h"
#include "frame_driver.h"
#include "frame_inputs.h"
#include "host_sink.h"
#include "mesh.h"
#include "parser_host.h"
#include "sound_analysis.h"
#include "state.h"
#include "state_vectors.h"
#include "time_model.h"

using render_core::FrameDriver;
using render_core::FrameUpdateInput;
using render_core::FrameUpdateOutput;
using render_core::TimeModel;

constexpr int kBeatdropPcmWindow = 2304;
constexpr int kBeatdropWaveformSamples = 576;
constexpr int kBeatdropResourceTextBytes = 32768;
constexpr double kBeatdropTicksPerSecond = 10000000.0;
constexpr double kBeatdropDeterministicFrameSeconds = 1.0 / 60.0;
constexpr int kBeatdropDefaultGridX = 64;
constexpr int kBeatdropDefaultGridY = 48;
constexpr float kBeatdropDefaultFps = 60.0f;
constexpr float kBeatdropTimeVariableResetDelay = 250000.0f;
constexpr bool kBeatdropScreenDependentRenderMode = true;

struct BdGlPipeline;

struct sv_context
{
    sv_config config;

    std::string resourcePath;
    std::string textureSearchPath;
    std::string cachePath;
    std::string lastError;

    CState* state;
    CState* oldState;

    BdHostSink hostSink;

    TimeModel timeModel;
    BdRawClock rawClock;
    FrameDriver frameDriver;

    int frame;
    float startTime;
    float presetStartTime;
    float nextPresetTime;
    bool nextPresetTimeWasInitialized;
    float snapPoint;

    float lastBlendSeconds;
    double lastFrameDt;
    uint32_t lastTargetFbo;

    int width;
    int height;
    int texsizeX;
    int texsizeY;
    int texsizeModeX;
    int texsizeModeY;
    int meshX;
    int meshY;
    float fpsHint;

    bool deterministic;
    bool pinDate;
    bool pinMouse;
    bool paused;

    float aspectX;
    float aspectY;
    float invAspectX;
    float invAspectY;

    std::vector<render_core::Vertex> meshVerts;
    std::vector<render_core::VertInfo> meshVertInfo;
    std::vector<int> meshIndices;
    int nGridX;
    int nGridY;

    BdGlPipeline* gl;
    int framesSinceResize;

    Vec4f randFrame;
    std::mt19937_64 rng;

    float year;
    float month;
    float day;
    float weekday;
    float hour;
    float minute;
    float second;
    float milliseconds;
    float totalSeconds;

    float mouseX;
    float mouseY;
    bool mouseDown;
    int mouseClicked;

    td_soundinfo sound;
    td_mysounddata mysound;
    FFT fft;
    FFT fftShader;
    float fftSmoothed[MY_FFT_SHADER_SAMPLES];
    float fftPeak[MY_FFT_SHADER_SAMPLES];
    int nFftPeakHold[MY_FFT_SHADER_SAMPLES];
    bool freezeBeatDetection;
    float frozenBeatDetection[6];

    AudioAnalysisSettings analysisSettings;

    float oldWave[2][kBeatdropWaveformSamples];
    bool oldWaveInitialized;
    int prevAlignOffset[2];
    bool alignWeightsReady;

    char shaderIncludeText[kBeatdropResourceTextBytes];
    char defaultWarpShaderText[kBeatdropResourceTextBytes];
    char defaultCompShaderText[kBeatdropResourceTextBytes];

    unsigned char pcmLeft[kBeatdropPcmWindow];
    unsigned char pcmRight[kBeatdropPcmWindow];
};

std::wstring BdUtf8ToWide(const char* utf8);
std::string BdWideToUtf8(const wchar_t* wide);
bool BdFileExists(const wchar_t* path);
void BdCopyString(char* dst, int dstBytes, const char* src);
void BdSampleContextSources(sv_context* ctx);
FrameInputs BdMakeFrameInputs(sv_context* ctx);
void BdDrawRandFrame(sv_context* ctx);

static_assert(sizeof(sv_context) > 0, "sv_context must be complete");

#endif