//
//  frame.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "context.h"
#include "gl_pipeline.h"

#include <cmath>
#include <cstring>

#include "equation_drivers.h"
#include "parser_win32.h"
#include "rng_compat.h"

namespace
{
    constexpr int kBeatdropAlignSamples = 480;
    constexpr int kBeatdropAlignMaxOctaves = 10;

    constexpr float kBeatdropBlendTimeAuto = 2.7f;
    constexpr float kBeatdropTimeBetweenPresets = 16.0f;
    constexpr float kBeatdropTimeBetweenPresetsRand = 10.0f;
    constexpr bool kBeatdropHardCutsDisabled = true;
    constexpr float kBeatdropHardCutLoudnessThresh = 2.5f;
    constexpr float kBeatdropHardCutHalflife = 60.0f;

    int BdWeekdayFromDate(int year, int month, int day)
    {
        int y = year;
        int m = month;
        y -= (m <= 2) ? 1 : 0;

        const int era = (y >= 0 ? y : y - 399) / 400;
        const unsigned int yoe = static_cast<unsigned int>(y - era * 400);
        const unsigned int doy = (153u * static_cast<unsigned int>(m + (m > 2 ? -3 : 9)) + 2u) / 5u
            + static_cast<unsigned int>(day) - 1u;
        const unsigned int doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;

        const long long days = static_cast<long long>(era) * 146097LL + static_cast<long long>(doe) - 719468LL;
        const int sundayZero = static_cast<int>(((days % 7LL) + 11LL) % 7LL);

        return (sundayZero == 0) ? 7 : sundayZero;
    }

    void BdAlignWaves(sv_context* ctx)
    {
        int alignOffset[2] = { 0, 0 };

        int octaves = static_cast<int>(floorf(logf(static_cast<float>(kBeatdropWaveformSamples - kBeatdropAlignSamples))
            / logf(2.0f)));

        if (octaves >= 4)
        {
            if (octaves > kBeatdropAlignMaxOctaves)
                octaves = kBeatdropAlignMaxOctaves;

            thread_local float tempWeight[kBeatdropAlignMaxOctaves][kBeatdropWaveformSamples];
            thread_local int firstNonzeroWeight[kBeatdropAlignMaxOctaves];
            thread_local int lastNonzeroWeight[kBeatdropAlignMaxOctaves];

            for (int ch = 0; ch < 2; ch++)
            {
                float tempNew[kBeatdropAlignMaxOctaves][kBeatdropWaveformSamples];
                float tempOld[kBeatdropAlignMaxOctaves][kBeatdropWaveformSamples];
                int spls[kBeatdropAlignMaxOctaves];
                int space[kBeatdropAlignMaxOctaves];

                memcpy(tempNew[0], ctx->sound.fWaveform[ch], sizeof(float) * kBeatdropWaveformSamples);
                memcpy(tempOld[0], &ctx->oldWave[ch][ctx->prevAlignOffset[ch]],
                    sizeof(float) * kBeatdropAlignSamples);
                spls[0] = kBeatdropWaveformSamples;
                space[0] = kBeatdropWaveformSamples - kBeatdropAlignSamples;

                for (int octave = 1; octave < octaves; octave++)
                {
                    spls[octave] = spls[octave - 1] / 2;
                    space[octave] = space[octave - 1] / 2;

                    for (int n = 0; n < spls[octave]; n++)
                    {
                        tempNew[octave][n] = 0.5f * (tempNew[octave - 1][n * 2] + tempNew[octave - 1][n * 2 + 1]);
                        tempOld[octave][n] = 0.5f * (tempOld[octave - 1][n * 2] + tempOld[octave - 1][n * 2 + 1]);
                    }
                }

                if (!ctx->alignWeightsReady)
                {
                    for (int octave = 0; octave < octaves; octave++)
                    {
                        const int compareSamples = spls[octave] - space[octave];

                        for (int n = 0; n < compareSamples; n++)
                        {
                            if (n < compareSamples / 2)
                                tempWeight[octave][n] = n * 2 / static_cast<float>(compareSamples);
                            else
                                tempWeight[octave][n] = (compareSamples - 1 - n) * 2 / static_cast<float>(compareSamples);

                            tempWeight[octave][n] = (tempWeight[octave][n] - 0.8f) * 5.0f + 0.8f;
                            if (tempWeight[octave][n] > 1.0f)
                                tempWeight[octave][n] = 1.0f;
                            if (tempWeight[octave][n] < 0.0f)
                                tempWeight[octave][n] = 0.0f;
                        }

                        int firstN = 0;
                        while (tempWeight[octave][firstN] == 0.0f && firstN < compareSamples)
                            firstN++;
                        firstNonzeroWeight[octave] = firstN;

                        int lastN = compareSamples - 1;
                        while (tempWeight[octave][lastN] == 0.0f && lastN >= 0)
                            lastN--;
                        lastNonzeroWeight[octave] = lastN;
                    }

                    ctx->alignWeightsReady = true;
                }

                int n1 = 0;
                int n2 = space[octaves - 1];

                for (int octave = octaves - 1; octave >= 0; octave--)
                {
                    const int compareSamples = spls[octave] - space[octave];
                    int lowestErrOffset = -1;
                    float lowestErrAmount = 0.0f;

                    for (int n = n1; n < n2; n++)
                    {
                        float errSum = 0.0f;

                        for (int i = firstNonzeroWeight[octave]; i <= lastNonzeroWeight[octave]; i++)
                        {
                            float x = (tempNew[octave][i + n] - tempOld[octave][i]) * tempWeight[octave][i];
                            if (x > 0.0f)
                                errSum += x;
                            else
                                errSum -= x;
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
                        if (n1 < 0)
                            n1 = 0;
                        if (n2 > space[octave - 1])
                            n2 = space[octave - 1];
                    }
                    else
                    {
                        alignOffset[ch] = lowestErrOffset;
                    }

                    (void)compareSamples;
                }
            }
        }

        memcpy(ctx->oldWave[0], ctx->sound.fWaveform[0], sizeof(float) * kBeatdropWaveformSamples);
        memcpy(ctx->oldWave[1], ctx->sound.fWaveform[1], sizeof(float) * kBeatdropWaveformSamples);
        ctx->prevAlignOffset[0] = alignOffset[0];
        ctx->prevAlignOffset[1] = alignOffset[1];

        for (int ch = 0; ch < 2; ch++)
        {
            if (alignOffset[ch] > 0)
            {
                for (int i = 0; i < kBeatdropAlignSamples; i++)
                    ctx->sound.fWaveform[ch][i] = ctx->sound.fWaveform[ch][i + alignOffset[ch]];

                memset(&ctx->sound.fWaveform[ch][kBeatdropAlignSamples], 0,
                    (kBeatdropWaveformSamples - kBeatdropAlignSamples) * sizeof(float));
            }
        }
    }

    float BdDrawRandom(sv_context* ctx)
    {
        return BdUniformRealDistribution<float>(0.0f, 1.0f)(ctx->rng);
    }
}

void BdDrawRandFrame(sv_context* ctx)
{
    const float w = (BdRand() % 7381) / 7380.0f;
    const float z = (BdRand() % 7381) / 7380.0f;
    const float y = (BdRand() % 7381) / 7380.0f;
    const float x = (BdRand() % 7381) / 7380.0f;
    ctx->randFrame = Vec4f(x, y, z, w);
}

void BdSampleContextSources(sv_context* ctx)
{
    if (ctx->pinDate)
    {
        ctx->year = ctx->config.year;
        ctx->month = ctx->config.month;
        ctx->day = ctx->config.day;
        ctx->weekday = ctx->config.weekday;
        ctx->hour = ctx->config.hour;
        ctx->minute = ctx->config.minute;
        ctx->second = ctx->config.second;
        ctx->milliseconds = ctx->config.milliseconds;
        ctx->totalSeconds = ctx->config.hour * 3600.0f + ctx->config.minute * 60.0f
            + ctx->config.second + ctx->config.milliseconds * 0.001f;
    }
    else if (ctx->deterministic)
    {
        ctx->year = 2000.0f;
        ctx->month = 1.0f;
        ctx->day = 1.0f;
        ctx->weekday = 6.0f;
        ctx->hour = 0.0f;
        ctx->minute = 0.0f;
        ctx->second = 0.0f;
        ctx->milliseconds = 0.0f;
        ctx->totalSeconds = 0.0f;
    }
    else
    {
        ParserLocalTime localTime = {};
        ParserGetLocalTime(&localTime);

        ctx->year = static_cast<float>(localTime.year);
        ctx->month = static_cast<float>(localTime.month);
        ctx->day = static_cast<float>(localTime.day);
        ctx->weekday = static_cast<float>(BdWeekdayFromDate(localTime.year, localTime.month, localTime.day));
        ctx->hour = static_cast<float>(localTime.hour);
        ctx->minute = static_cast<float>(localTime.minute);
        ctx->second = static_cast<float>(localTime.second);
        ctx->milliseconds = 0.0f;
        ctx->totalSeconds = ctx->hour * 3600.0f + ctx->minute * 60.0f + ctx->second;
    }

    if (ctx->pinMouse)
    {
        ctx->mouseX = ctx->config.mouse_x;
        ctx->mouseY = ctx->config.mouse_y;
    }
    else if (ctx->deterministic)
    {
        ctx->mouseX = -1.0f;
        ctx->mouseY = -1.0f;
    }
    else
    {
        ctx->mouseX = 0.0f;
        ctx->mouseY = 0.0f;
    }

    ctx->mouseDown = false;
    if (ctx->mouseClicked > 0)
        ctx->mouseClicked--;
}

FrameInputs BdMakeFrameInputs(sv_context* ctx)
{
    const float time = static_cast<float>(ctx->timeModel.GetTime());
    const float fps = ctx->timeModel.GetFps();

    return FrameInputs{
        ctx->mysound,
        time,
        fps,
        ctx->frame,
        ctx->startTime,
        ctx->presetStartTime,
        ctx->nextPresetTime,
        ctx->nGridX,
        ctx->nGridY,
        ctx->width,
        ctx->height,
        kBeatdropScreenDependentRenderMode,
        ctx->invAspectX,
        ctx->invAspectY,
        ctx->mouseX,
        ctx->mouseY,
        ctx->mouseDown,
        ctx->mouseClicked,
        false,
        false,
        false,
        false,
        ctx->year,
        ctx->month,
        ctx->day,
        ctx->weekday,
        ctx->hour,
        ctx->minute,
        ctx->second,
        ctx->milliseconds,
        ctx->totalSeconds };
}

extern "C" void sv_render_frame(sv_context* ctx, double dt_seconds, uint32_t target_fbo)
{
    if (ctx == nullptr || ctx->paused)
        return;

    ctx->lastFrameDt = dt_seconds;
    ctx->lastTargetFbo = target_fbo;

    ctx->rawClock.Advance();
    ctx->timeModel.Step(ctx->frame, ctx->rawClock, kBeatdropTimeVariableResetDelay, false);

    const float time = static_cast<float>(ctx->timeModel.GetTime());
    const float fps = ctx->timeModel.GetFps();

    if (ctx->timeModel.TimeWasReset())
    {
        ctx->presetStartTime = time;
        ctx->nextPresetTime = -1.0f;
        ctx->nextPresetTimeWasInitialized = false;
    }

    GetAudioBuf(ctx->pcmLeft, ctx->pcmRight, kBeatdropPcmWindow);
    AnalyzeNewSoundAudio(ctx->pcmLeft, ctx->pcmRight, fps, ctx->sound, ctx->fft);
    BdAlignWaves(ctx);

    ComputeCustomSoundAnalysis(ctx->analysisSettings, fps, ctx->frame,
        ctx->state->m_fFFTAttack.eval(time), ctx->state->m_fFFTDecay.eval(time),
        ctx->freezeBeatDetection, ctx->frozenBeatDetection, ctx->sound, ctx->mysound,
        ctx->fft, ctx->fftShader, ctx->fftSmoothed, ctx->fftPeak, ctx->nFftPeakHold);

    BdSampleContextSources(ctx);

    float nextPresetTimeRandomDraw = 0.0f;
    if (ctx->nextPresetTime < 0.0f)
        nextPresetTimeRandomDraw = BdDrawRandom(ctx);

    FrameUpdateInput frameDriverInput = {};
    frameDriverInput.frame = ctx->frame;
    frameDriverInput.redraw = false;
    frameDriverInput.time = static_cast<double>(time);
    frameDriverInput.fps = fps;
    frameDriverInput.nextPresetTime = ctx->nextPresetTime;
    frameDriverInput.nextPresetTimeRandomDraw = nextPresetTimeRandomDraw;
    frameDriverInput.timeBetweenPresetsRand = kBeatdropTimeBetweenPresetsRand;
    frameDriverInput.blendTimeAuto = kBeatdropBlendTimeAuto;
    frameDriverInput.timeBetweenPresets = kBeatdropTimeBetweenPresets;
    frameDriverInput.isBlending = ctx->state->m_bBlending;
    frameDriverInput.blendStartTime = ctx->state->m_fBlendStartTime;
    frameDriverInput.blendDuration = ctx->state->m_fBlendDuration;
    frameDriverInput.hardCutsDisabled = kBeatdropHardCutsDisabled;
    frameDriverInput.presetLockedByUser = false;
    frameDriverInput.presetLockedByCode = false;
    frameDriverInput.hardCutLoudnessThresh = kBeatdropHardCutLoudnessThresh;
    frameDriverInput.hardCutHalflife = kBeatdropHardCutHalflife;
    frameDriverInput.immRel[0] = ctx->mysound.imm_rel[0];
    frameDriverInput.immRel[1] = ctx->mysound.imm_rel[1];
    frameDriverInput.immRel[2] = ctx->mysound.imm_rel[2];
    frameDriverInput.waveScale = ctx->state->m_fWaveScale.eval(time);
    frameDriverInput.waveSmoothing = ctx->state->m_fWaveSmoothing.eval(time);
    frameDriverInput.oldWarpPSVersion = ctx->oldState->m_nWarpPSVersion;
    frameDriverInput.newWarpPSVersion = ctx->state->m_nWarpPSVersion;
    frameDriverInput.oldCompPSVersion = ctx->oldState->m_nCompPSVersion;
    frameDriverInput.newCompPSVersion = ctx->state->m_nCompPSVersion;

    FrameUpdateOutput frameDriverOutput;
    ctx->frameDriver.Update(frameDriverInput, ctx->mysound.fWave, frameDriverOutput);

    if (frameDriverOutput.isFirstFrame)
    {
        ctx->startTime = time;
        ctx->presetStartTime = time;
    }

    ctx->nextPresetTime = frameDriverOutput.nextPresetTime;
    ctx->nextPresetTimeWasInitialized = frameDriverOutput.nextPresetTimeWasInitialized;

    BdDrawRandFrame(ctx);

    if (frameDriverOutput.blendProgressComputed)
    {
        ctx->state->m_fBlendProgress = frameDriverOutput.blendProgress;

        if (frameDriverOutput.blendFinished)
            ctx->state->m_bBlending = false;
    }

    RunPerFrameEquations(ctx->state, ctx->oldState, frameDriverOutput.shaderBlendCode,
        BdMakeFrameInputs(ctx), ctx->snapPoint);

    BdGlRenderFrame(ctx, target_fbo);

    ctx->frame++;
}