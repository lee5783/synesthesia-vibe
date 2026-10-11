//
//  sound_analysis.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-29.
//

#include "sound_analysis.h"
#include <algorithm>
#include <math.h>
#include <cstring>

int SAMPLE_RATE = 44100; // Default sample rate used by the visualizer FFT.

void GetAudioBufFloat(float* pWaveL, float* pWaveR, int SamplesCount);

float AdjustRateToFPS(float per_frame_decay_rate_at_fps1, float fps1, float actual_fps)
{
    // returns the equivalent per-frame decay rate at actual_fps

    // basically, do all your testing at fps1 and get a good decay rate;
    // then, in the real application, adjust that rate by the actual fps each time you use it.

    float per_second_decay_rate_at_fps1 = powf(per_frame_decay_rate_at_fps1, fps1);
    float per_frame_decay_rate_at_fps2 = powf(per_second_decay_rate_at_fps1, 1.0f/actual_fps);

    return per_frame_decay_rate_at_fps2;
}

void AnalyzeNewSoundAudio(unsigned char *pWaveL, unsigned char *pWaveR, float fps, td_soundinfo &sound, FFT &fft)
{
	// we get 576 samples in from winamp.
	// the output of the fft has 'num_frequencies' samples,
	//   and represents the frequency range 0 hz - 22,050 hz.
	// usually, plugins only use half of this output (the range 0 hz - 11,025 hz),
	//   since >10 khz doesn't usually contribute much.

	int i;
	int ch;

	float temp_wave[2][576];

	int old_i = 0;
	for (i=0; i<576; i++)
	{
		sound.fWaveform[0][i] = (float)((pWaveL[i] ^ 128) - 128);
		sound.fWaveform[1][i] = (float)((pWaveR[i] ^ 128) - 128);

		// simulating single frequencies from 200 to 11,025 Hz:
		//float freq = 1.0f + 11050*(GetFrame() % 100)*0.01f;
		//m_sound.fWaveform[0][i] = 10*sinf(i*freq*6.28f/44100.0f);

		// damp the input into the FFT a bit, to reduce high-frequency noise:
		temp_wave[0][i] = 0.5f*(sound.fWaveform[0][i] + sound.fWaveform[0][old_i]);
		temp_wave[1][i] = 0.5f*(sound.fWaveform[1][i] + sound.fWaveform[1][old_i]);
		old_i = i;
	}

	fft.time_to_frequency_domain(temp_wave[0], sound.fSpectrum[0]);
	fft.time_to_frequency_domain(temp_wave[1], sound.fSpectrum[1]);

	// sum (left channel) spectrum up into 3 bands
	// [note: the new ranges do it so that the 3 bands are equally spaced, pitch-wise]
	float min_freq = 20.0f;
	float max_freq = 20000.0f;
	float net_octaves = (logf(max_freq/min_freq) / logf(2.0f));     // 5.7846348455575205777914165223593
	float octaves_per_band = net_octaves / 3.0f;                    // 1.9282116151858401925971388407864
	float mult = powf(2.0f, octaves_per_band); // each band's highest freq. divided by its lowest freq.; 3.805831305510122517035102576162
	// [to verify: min_freq * mult * mult * mult should equal max_freq.]
	for (ch=0; ch<2; ch++)
	{
		for (i=0; i<3; i++)
		{
			// old guesswork code for this:
			//   float exp = 2.1f;
			//   int start = (int)(NUM_FREQUENCIES*0.5f*powf(i/3.0f, exp));
			//   int end   = (int)(NUM_FREQUENCIES*0.5f*powf((i+1)/3.0f, exp));
			// results:
			//          old range:      new range (ideal):
			//   bass:  0-1097          200-761
			//   mids:  1097-4705       761-2897
			//   treb:  4705-11025      2897-11025
			int start = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)i)/max_freq);
			int end   = (int)(NUM_FREQUENCIES * min_freq*powf(mult, (float)(i+1))/max_freq);
			if (start < 0) start = 0;
			if (end > NUM_FREQUENCIES) end = NUM_FREQUENCIES;

			sound.imm[ch][i] = 0;
			for (int j=start; j<end; j++)
				sound.imm[ch][i] += sound.fSpectrum[ch][j];
			sound.imm[ch][i] /= (float)(end-start);
		}
	}

	// multiply by long-term, empirically-determined inverse averages:
	// (for a trial of 244 songs, 10 seconds each, somewhere in the 2nd or 3rd minute,
	//  the average levels were: 0.326781557	0.38087377	0.199888934
	for (ch=0; ch<2; ch++)
	{
		sound.imm[ch][0] /= 0.326781557f;//0.270f;
		sound.imm[ch][1] /= 0.380873770f;//0.343f;
		sound.imm[ch][2] /= 0.199888934f;//0.295f;
	}

	// do temporal blending to create attenuated and super-attenuated versions
	for (ch=0; ch<2; ch++)
	{
		for (i=0; i<3; i++)
		{
			// m_sound.avg[i]
			{
				float avg_mix;
				if (sound.imm[ch][i] > sound.avg[ch][i])
					avg_mix = AdjustRateToFPS(0.2f, 14.0f, fps);
				else
					avg_mix = AdjustRateToFPS(0.5f, 14.0f, fps);
				sound.avg[ch][i] = sound.avg[ch][i]*avg_mix + sound.imm[ch][i]*(1-avg_mix);
			}

			// m_sound.med_avg[i]
			// m_sound.long_avg[i]
			{
				float med_mix  = 0.91f;//0.800f + 0.11f*powf(t, 0.4f);    // primarily used for velocity_damping
				float long_mix = 0.96f;//0.800f + 0.16f*powf(t, 0.2f);    // primarily used for smoke plumes
				med_mix  = AdjustRateToFPS(med_mix, 14.0f, fps);
				long_mix = AdjustRateToFPS(long_mix, 14.0f, fps);
				sound.med_avg[ch][i]  =  sound.med_avg[ch][i]*(med_mix) + sound.imm[ch][i]*(1-med_mix);
				sound.long_avg[ch][i] = sound.long_avg[ch][i]*(long_mix) + sound.imm[ch][i]*(1-long_mix);
			}
		}
	}
}

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
    int nFFTPeakHold[MY_FFT_SHADER_SAMPLES])
{
	//Now uses configurations via beatdrop.ini, don't modify here.
    //Bass
    int BASS_MIN = settings.bassStart;
    int BASS_MAX = settings.bassEnd;

    //Middle
    int MID_MIN = settings.midStart;
    int MID_MAX = settings.midEnd;

    //Treble
    int TREBLE_MIN = settings.trebStart;
    int TREBLE_MAX = settings.trebEnd;

    // This uses the sample rate dependent on your speaker device.
    // Beat Detection Configuration
    // Look at the start of line 10566 for the new beat detection splitting algorithm.

    // The simple-wave renderer exposes the captured 576-sample waveform.
    // Keep that buffer bounded; the FFT below uses a larger zero-padded window.
    memcpy(mysound.fWave[0], sound.fWaveform[0], sizeof(mysound.fWave[0]));
    memcpy(mysound.fWave[1], sound.fWaveform[1], sizeof(mysound.fWave[1]));

    // do our own [UN-NORMALIZED] fft
    // m_sound.fWaveform contains 576 samples, while MY_FFT_WINDOW is 1024.
    // Zero-padding the tail preserves the existing FFT size without reading
    // beyond the captured waveform and avoids corrupting simple waveforms.
    float fWaveLeft[MY_FFT_WINDOW] = {};
    float fWaveRight[MY_FFT_WINDOW] = {};
    memcpy(fWaveLeft, sound.fWaveform[0], sizeof(sound.fWaveform[0]));
    memcpy(fWaveRight, sound.fWaveform[1], sizeof(sound.fWaveform[1]));

    //float fWaveLeft[MY_FFT_WINDOW];
    //float fWaveRight[MY_FFT_WINDOW];
    //GetAudioBufFloat(fWaveLeft, fWaveRight, MY_FFT_WINDOW);
    //for (int i = 0; i < MY_FFT_WINDOW; i++)
    //{
    //    fWaveLeft[i] *= g_fAudioSensitivity;
    //    fWaveRight[i] *= g_fAudioSensitivity;
    //}

	memset(mysound.fSpecLeft, 0, sizeof(float)*MY_FFT_SAMPLES);
    memset(mysound.fSpecRight, 0, sizeof(float)*MY_FFT_SAMPLES);

	myfft.time_to_frequency_domain(fWaveLeft, mysound.fSpecLeft);
    myfft.time_to_frequency_domain(fWaveRight, mysound.fSpecRight);
	//for (i=0; i<MY_FFT_SAMPLES; i++) fSpecLeft[i] = sqrtf(fSpecLeft[i]*fSpecLeft[i] + fSpecTemp[i]*fSpecTemp[i]);

    float fWaveLargeLeft[MY_FFT_SHADER_WINDOW];
    float fWaveLargeRight[MY_FFT_SHADER_WINDOW];
    GetAudioBufFloat(fWaveLargeLeft, fWaveLargeRight, MY_FFT_SHADER_WINDOW);
    // for (int i = 0; i < MY_FFT_SHADER_WINDOW; i++)
    // {
    //     fWaveLargeLeft[i] *= g_fAudioSensitivity;
    //     fWaveLargeRight[i] *= g_fAudioSensitivity;
    // }

    // Apply FFT smoothing and upload to GPU texture for get_fft()/get_fft_hz() shader functions
    // Compute clean (un-equalized) FFT for get_fft()/get_fft_hz() shader functions
    float fShaderSpecLeft[MY_FFT_SHADER_SAMPLES];
    float fShaderSpecRight[MY_FFT_SHADER_SAMPLES];
    memset(fShaderSpecLeft, 0, sizeof(float) * MY_FFT_SHADER_SAMPLES);
    memset(fShaderSpecRight, 0, sizeof(float) * MY_FFT_SHADER_SAMPLES);
    myfftshader.time_to_frequency_domain(fWaveLargeLeft, fShaderSpecLeft);
    myfftshader.time_to_frequency_domain(fWaveLargeRight, fShaderSpecRight);

    // RMS normalization
    // Sum all the bins and take a square mono calculated FFT.
    float fftSumSq = 0;
    for (int i = 0; i < MY_FFT_SHADER_SAMPLES; i++)
    {
        float mono = (fShaderSpecLeft[i] + fShaderSpecRight[i]) * .5f;
        fftSumSq += mono * mono;
    }
    float fftRMS = sqrtf(fftSumSq / MY_FFT_SHADER_SAMPLES);    // Calculate RMS from square mono calculated FFT.
    if (fftRMS < 1e-5f) fftRMS = 1e-5f; // PREVENT DIVISION BY ZERO ERROR, is it neccesary?

    // Smoothing RMS with simple low-pass
    static float s_fftRMS = .1f; // Initialize with a sensible default
    if (fps < 1e-8f) fps = 1e-8f;   // PREVENT DIVISION BY ZERO ERROR
    float tauRMS; // smoothing time constant
    if (fftRMS > s_fftRMS)
        tauRMS = 2; // Fast adaption for loud environment
    else tauRMS = 5; // Slowly decreases for quiet parts
    float RMSDecay = expf(-1 / fps / tauRMS);
    s_fftRMS = s_fftRMS * RMSDecay + fftRMS * (1-RMSDecay);

    float fftScaling = settings.fftShaderScaling;   // FFT scaling is now controlled via .ini setting file.

    // Apply FFT smoothing and upload to GPU texture
    {
        float attack = fftAttack;  // Reads FFT Attack from state
        float decay  = fftDecay;   // Reads FFT Decay from state
        float decayFactor = (1.0f - decay) * (1.0f - decay);

        // FPS-independent: scale both coefficients relative to 60fps reference
        float attackPerFrame = 1.0f - powf(1.0f - attack,      60.0f / fps);
        float decayPerFrame  = 1.0f - powf(1.0f - decayFactor, 60.0f / fps);
        const float kVisibleFloor = 5e-8f;
        for (int fi = 0; fi < MY_FFT_SHADER_SAMPLES; fi++)
        {
            float mono = (fShaderSpecLeft[fi] + fShaderSpecRight[fi]) * 0.5f;
            // Normalize FFT with RMS.
            mono = (mono / s_fftRMS) * fftScaling;

            mono -= settings.fftShaderNoiseFloor;
            if (mono < 0.0f) mono = 0.0f;
            // Attenuate low-frequency bins to reduce bass over-accentuation.
            // Bins below ~2000 Hz (bin ~24) are progressively reduced.
            // The curve ramps from 0.15 at bin 0 to 1.0 at bin 24.
            const int kBassRampEnd = 24;
            if (fi < kBassRampEnd)
            {
                float t = (float)fi / (float)kBassRampEnd;
                float lowcut = 0.15f + 0.85f * (t*t);
                mono *= lowcut;
            }
            if (mono > fFFTSmoothed[fi])
                fFFTSmoothed[fi] += (mono - fFFTSmoothed[fi]) * attackPerFrame;
            else
                fFFTSmoothed[fi] += (mono - fFFTSmoothed[fi]) * decayPerFrame;
            if (fFFTSmoothed[fi] < kVisibleFloor)
                fFFTSmoothed[fi] = 0.0f;
        }
        // Update peak hold: hold for 1 second then decay
        for (int fi = 0; fi < MY_FFT_SHADER_SAMPLES; fi++)
        {
            if (fFFTSmoothed[fi] >= fFFTPeak[fi])
            {
                fFFTPeak[fi] = fFFTSmoothed[fi];
                nFFTPeakHold[fi] = fps * (.25 + (decay*.75)); // FPS-independent; decay-controllable peak falling
            }
            else if (nFFTPeakHold[fi] > 0)
                nFFTPeakHold[fi]--;
            else
            {
                fFFTPeak[fi] *= exp(-1/fps/.3); // ~3% drop per frame, creates that smooth gravity-fall effect, FPS independent
                //if (m_fFFTPeak[fi] < kVisibleFloor) m_fFFTPeak[fi] = 0.0f;
                if (fFFTPeak[fi] < 0.0f) fFFTPeak[fi] = 0.0f;
            }
        }
    }

	// sum spectrum up into 3 bands
    //DeepSeek - Updated Beat Detection Splitting Algorithm

    // Frequency bin size
    float bin_size = (SAMPLE_RATE / 2.0f) / MY_FFT_SAMPLES;

    for (int i = 0; i < 3; i++)
    {
        // Process each frequency band
        int freq_min, freq_max;

        switch (i) {
        case 0: // Bass
            freq_min = BASS_MIN;
            freq_max = BASS_MAX;
            break;
        case 1: // Mid
            freq_min = MID_MIN;
            freq_max = MID_MAX;
            break;
        case 2: // Treble
            freq_min = TREBLE_MIN;
            freq_max = TREBLE_MAX;
            break;
        }

        // Calculate exact bin ranges without clamping
        int start_bin = (int)(freq_min / bin_size);
        int end_bin = (int)(freq_max / bin_size);

        // Ensure we stay within FFT bounds naturally
        start_bin = std::max(0, start_bin);
        end_bin = std::min(MY_FFT_SAMPLES - 1, end_bin);

        mysound.imm[i] = 0; //To prevent the waveform's spikyness and performance lag

        // Sum the energy in the frequency range
        for (int j = start_bin; j <= end_bin; j++)
        {
            mysound.imm[i] += (mysound.fSpecLeft[j] + mysound.fSpecRight[j])/2;
        }
    }

	// do temporal blending to create attenuated and super-attenuated versions
	for (int i=0; i<3; i++)
	{
        float rate;

		if (mysound.imm[i] > mysound.avg[i])
			rate = 0.2f;
		else
			rate = 0.5f;
        rate = AdjustRateToFPS(rate, 30.0f, fps);
        mysound.avg[i] = mysound.avg[i]*rate + mysound.imm[i]*(1-rate);

		if (frame < 50)
			rate = 0.9f;
		else
			rate = 0.992f;
        rate = AdjustRateToFPS(rate, 30.0f, fps);
        mysound.long_avg[i] = mysound.long_avg[i]*rate + mysound.imm[i]*(1-rate);

        // also get bass/mid/treble levels *relative to the past*
        //changed all the values to 0 instead of 1 when it's no music
        if (fabsf(mysound.long_avg[i]) < 0.001f)
            mysound.imm_rel[i] = 0.0f;
        else
            mysound.imm_rel[i] = mysound.imm[i] / mysound.long_avg[i];

        if (fabsf(mysound.long_avg[i]) < 0.001f)
            mysound.avg_rel[i] = 0.0f;
        else
            mysound.avg_rel[i] = mysound.avg[i] / mysound.long_avg[i];
	}

    // Keep every consumer of the six public beat variables on the same frozen
    // sample. The raw spectrum and waveform remain live while frozen.
    if (freezeBeatDetection)
    {
        for (int i = 0; i < 3; i++)
        {
            mysound.imm_rel[i] = frozenBeatDetection[i];
            mysound.avg_rel[i] = frozenBeatDetection[i + 3];
        }
    }
}
