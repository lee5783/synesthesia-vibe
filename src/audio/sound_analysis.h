//
//  sound_analysis.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#ifndef __BEATDROP_AUDIO_SOUND_ANALYSIS_H__
#define __BEATDROP_AUDIO_SOUND_ANALYSIS_H__ 1

#include "fft.h"
#include "analysis_settings.h"

#define NUM_FREQUENCIES 512

#define MY_FFT_SAMPLES 512     // for old [pre-vms] milkdrop sound analysis
#define MY_FFT_WINDOW MY_FFT_SAMPLES*2
#define MY_FFT_SHADER_SAMPLES 1024
#define MY_FFT_SHADER_WINDOW MY_FFT_SHADER_SAMPLES*2

typedef struct
{
    float imm[2][3];                        // bass, mids, treble, no damping, for each channel (long-term average is 1)
    float avg[2][3];                        // bass, mids, treble, some damping, for each channel (long-term average is 1)
    float med_avg[2][3];                    // bass, mids, treble, more damping, for each channel (long-term average is 1)
    float long_avg[2][3];                   // bass, mids, treble, heavy damping, for each channel (long-term average is 1)
    float infinite_avg[2][3];               // bass, mids, treble: winamp's average output levels. (1)
    float fWaveform[2][576];                // Not all 576 are valid! - only NUM_WAVEFORM_SAMPLES samples are valid for each channel (note: NUM_WAVEFORM_SAMPLES is declared in shell_defines.h)
    float fSpectrum[2][NUM_FREQUENCIES];    // NUM_FREQUENCIES samples for each channel (note: NUM_FREQUENCIES is declared in shell_defines.h)
} td_soundinfo;                             // ...range is 0 Hz to 22050 Hz, evenly spaced.

typedef struct
{
	float   imm[3];			// bass, mids, treble (absolute)
	float	imm_rel[3];		// bass, mids, treble (relative to song; 1=avg, 0.9~below, 1.1~above)
	float	avg[3];			// bass, mids, treble (absolute)
	float	avg_rel[3];		// bass, mids, treble (relative to song; 1=avg, 0.9~below, 1.1~above)
	float	long_avg[3];	// bass, mids, treble (absolute)
    float   fWave[2][576];
    float   fSpecLeft[MY_FFT_SAMPLES];
	float   fSpecRight[MY_FFT_SAMPLES];
} td_mysounddata;

extern int SAMPLE_RATE;

float AdjustRateToFPS(float per_frame_decay_rate_at_fps1, float fps1, float actual_fps);

void AnalyzeNewSoundAudio(unsigned char *pWaveL, unsigned char *pWaveR, float fps, td_soundinfo &sound, FFT &fft);

void ComputeCustomSoundAnalysis(
    const AudioAnalysisSettings &settings,
    float fps,
    int frame,
    float fftAttack,
    float fftDecay,
    bool freezeBeatDetection,
    const float frozenBeatDetection[6],
    const td_soundinfo &sound,
    td_mysounddata &mysound,
    FFT &myfft,
    FFT &myfftshader,
    float fFFTSmoothed[MY_FFT_SHADER_SAMPLES],
    float fFFTPeak[MY_FFT_SHADER_SAMPLES],
    int nFFTPeakHold[MY_FFT_SHADER_SAMPLES]);

#endif
