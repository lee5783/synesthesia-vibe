//
//  frame.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "context.h"
#include "gl_pipeline.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include "equation_drivers.h"
#include "gl_error.h"
#include "parser_win32.h"
#include "rng_compat.h"

namespace
{
    constexpr int kBeatdropAlignSamples = 480;
    constexpr int kBeatdropAlignMaxOctaves = 10;

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

namespace
{
    void BdStepSimulationClock(sv_context* ctx)
    {
        ctx->rawClock.Advance();
        ctx->timeModel.Step(ctx->frame, ctx->rawClock, kBeatdropTimeVariableResetDelay, false);

        if (ctx->timeModel.TimeWasReset())
        {
            ctx->presetStartTime = static_cast<float>(ctx->timeModel.GetTime());
            ctx->nextPresetTime = -1.0f;
            ctx->nextPresetTimeWasInitialized = false;
        }
    }

    void BdAnalyzeAudio(sv_context* ctx, float fps)
    {
        const float time = static_cast<float>(ctx->timeModel.GetTime());

        GetAudioBuf(ctx->pcmLeft, ctx->pcmRight, kBeatdropPcmWindow);
        AnalyzeNewSoundAudio(ctx->pcmLeft, ctx->pcmRight, fps, ctx->sound, ctx->fft);
        BdAlignWaves(ctx);

        ComputeCustomSoundAnalysis(ctx->analysisSettings, fps, ctx->frame,
            ctx->state->m_fFFTAttack.eval(time), ctx->state->m_fFFTDecay.eval(time),
            ctx->freezeBeatDetection, ctx->frozenBeatDetection, ctx->sound, ctx->mysound,
            ctx->fft, ctx->fftShader, ctx->fftSmoothed, ctx->fftPeak, ctx->nFftPeakHold);
    }

    void BdRunFrameEquations(sv_context* ctx)
    {
        const float time = static_cast<float>(ctx->timeModel.GetTime());
        const float fps = ctx->timeModel.GetFps();

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
        const sv_transition_config& transition = ctx->transition;
        const bool timerEnabled = transition.preset_duration > 0.0f;
        frameDriverInput.timeBetweenPresetsRand = timerEnabled ? transition.preset_duration_rand : kBeatdropProgressTimeBetweenPresetsRand;
        frameDriverInput.blendTimeAuto = transition.soft_cut_seconds;
        frameDriverInput.timeBetweenPresets = timerEnabled ? transition.preset_duration : kBeatdropProgressTimeBetweenPresets;
        frameDriverInput.isBlending = ctx->state->m_bBlending;
        frameDriverInput.blendStartTime = ctx->state->m_fBlendStartTime;
        frameDriverInput.blendDuration = ctx->state->m_fBlendDuration;
        frameDriverInput.hardCutsDisabled = !transition.hard_cuts_enabled;
        frameDriverInput.presetLockedByUser = false;
        frameDriverInput.presetLockedByCode = false;
        frameDriverInput.hardCutLoudnessThresh = transition.hard_cut_loudness;
        frameDriverInput.hardCutHalflife = transition.hard_cut_halflife;
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

        if (ctx->requestFn != nullptr && !ctx->requestLatched.load())
        {
            if (frameDriverOutput.hardCutTriggered)
            {
                ctx->requestPending = true;
                ctx->requestReason = SV_PRESET_REQUEST_HARD_CUT;
            }
            else if (frameDriverOutput.presetTimerExpired && timerEnabled && !ctx->requestPending)
            {
                ctx->requestPending = true;
                ctx->requestReason = SV_PRESET_REQUEST_TIMER;
            }
        }

        BdDrawRandFrame(ctx);

        if (frameDriverOutput.blendProgressComputed)
        {
            ctx->state->m_fBlendProgress = frameDriverOutput.blendProgress;

            if (frameDriverOutput.blendFinished)
                ctx->state->m_bBlending = false;
        }

        RunPerFrameEquations(ctx->state, ctx->oldState, frameDriverOutput.shaderBlendCode,
            BdMakeFrameInputs(ctx), ctx->snapPoint);
    }

    void BdConsumePresetLoadFailure(sv_context* ctx)
    {
        if (!ctx->requestLoadFailed.exchange(false))
            return;

        if (ctx->requestLatched.exchange(false))
        {
            ctx->nextPresetTime = -1.0f;
            ctx->nextPresetTimeWasInitialized = false;
        }
    }

    void BdDeliverPresetRequest(sv_context* ctx)
    {
        if (!ctx->requestPending)
            return;

        ctx->requestPending = false;

        const sv_preset_request_fn fn = ctx->requestFn;
        void* const user = ctx->requestUser;
        if (fn == nullptr || ctx->requestLatched.load())
            return;

        sv_preset_request request;
        request.reason = ctx->requestReason;
        request.suggested_blend_seconds = ctx->requestReason == SV_PRESET_REQUEST_HARD_CUT ? 0.0f : ctx->transition.soft_cut_seconds;

        ctx->requestLatched.store(true);
        fn(ctx, &request, user);
    }

    void BdRenderLegacyFrame(sv_context* ctx, uint32_t target_fbo)
    {
        BdStepSimulationClock(ctx);
        BdAnalyzeAudio(ctx, ctx->timeModel.GetFps());
        BdRunFrameEquations(ctx);

        BdGlRenderFrame(ctx, target_fbo);

        ctx->frame++;

        BdDeliverPresetRequest(ctx);
    }

    double BdWallSeconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    double BdHostFrameSeconds(sv_context* ctx, double dt_seconds)
    {
        const double now = BdWallSeconds();
        const double previous = ctx->lastWallSeconds;
        ctx->lastWallSeconds = now;

        if (ctx->resumePending)
        {
            ctx->resumePending = false;
            return 1.0 / static_cast<double>(ctx->fpsHint);
        }

        if (std::isfinite(dt_seconds) && dt_seconds > 0.0)
            return std::min(dt_seconds, kBeatdropMaxFrameDtSeconds);

        if (previous <= 0.0)
            return 1.0 / static_cast<double>(ctx->fpsHint);

        return std::min(std::max(now - previous, 0.0), kBeatdropMaxFrameDtSeconds);
    }

    double BdSimulationStepSeconds(sv_context* ctx)
    {
        const double rate = std::round(static_cast<double>(ctx->displayFps));
        return 1.0 / std::min(std::max(rate, kBeatdropMinSimulationRate), kBeatdropMaxSimulationRate);
    }

    int BdMaxSimulationSteps(sv_context* ctx)
    {
        const double rate = std::max(static_cast<double>(ctx->playbackRate), 1.0);
        return static_cast<int>(std::ceil(kBeatdropMaxSimulationStepsPerFrame * rate));
    }

    int BdTakeSimulationSteps(sv_context* ctx, double frameSeconds, double stepSeconds)
    {
        ctx->stepAccumulator += frameSeconds * static_cast<double>(ctx->playbackRate);

        int steps = static_cast<int>((ctx->stepAccumulator + kBeatdropSimulationStepTolerance * stepSeconds) / stepSeconds);

        if (ctx->frame == 0 && steps < 1)
            steps = 1;

        const int maxSteps = BdMaxSimulationSteps(ctx);
        if (steps > maxSteps)
        {
            ctx->stepAccumulator = 0.0;
            return maxSteps;
        }

        ctx->stepAccumulator = std::max(ctx->stepAccumulator - steps * stepSeconds, -kBeatdropSimulationStepTolerance * stepSeconds);
        return steps;
    }

    void BdUpdateDisplayFps(sv_context* ctx, double frameSeconds)
    {
        const float measured = static_cast<float>(1.0 / std::max(frameSeconds, 1.0 / 1000.0));

        if (ctx->frame == 0)
            ctx->displayFps = measured;
        else
            ctx->displayFps = kBeatdropDisplayFpsDamping * ctx->displayFps + (1.0f - kBeatdropDisplayFpsDamping) * measured;
    }

    void BdRenderFixedStepFrame(sv_context* ctx, double dt_seconds, uint32_t target_fbo)
    {
        const double frameSeconds = BdHostFrameSeconds(ctx, dt_seconds);
        BdUpdateDisplayFps(ctx, frameSeconds);

        const double stepSeconds = BdSimulationStepSeconds(ctx);
        const int steps = BdTakeSimulationSteps(ctx, frameSeconds, stepSeconds);
        ctx->rawClock.SetTickDelta(std::llround(stepSeconds * kBeatdropTicksPerSecond));

        BdAnalyzeAudio(ctx, ctx->displayFps);
        BdGlBeginFrame(ctx);

        for (int i = 0; i < steps; i++)
        {
            BdStepSimulationClock(ctx);
            BdRunFrameEquations(ctx);

            BdGlStepFrame(ctx);

            ctx->frame++;
        }

        BdGlPresentFrame(ctx, target_fbo);

        BdDeliverPresetRequest(ctx);
    }

    void BdRestartDebugWindow(sv_context* ctx, double now)
    {
        ctx->debugWindowPending = false;
        ctx->debugWindowStart = now;
        ctx->debugWindowCalls = 0;
        ctx->debugWindowFrame = ctx->frame;
    }

    void BdEmitDebugInfo(sv_context* ctx, double elapsed)
    {
        const sv_debug_fn fn = ctx->debugFn;
        void* const user = ctx->debugUser;
        const BdGlPipeline* gl = ctx->gl;

        const unsigned int glErrorTotal = render_gl::DrainedGlErrorTotal();
        const unsigned int glErrors = glErrorTotal - ctx->debugGlErrorBase;
        ctx->debugGlErrorBase = glErrorTotal;

        BdPresetInfoSnapshot presetInfo;
        {
            std::lock_guard<std::mutex> lock(ctx->presetInfoMutex);
            presetInfo = ctx->presetInfo;
        }

        sv_debug_info info;
        info.time = static_cast<double>(static_cast<float>(ctx->timeModel.GetTime()) - ctx->startTime);
        info.frame = static_cast<uint32_t>(ctx->frame);
        info.display_fps = static_cast<float>(ctx->debugWindowCalls / elapsed);
        info.sim_steps_per_frame = static_cast<float>(ctx->frame - ctx->debugWindowFrame) / static_cast<float>(ctx->debugWindowCalls);
        info.playback_rate = ctx->deterministic ? 1.0f : ctx->playbackRate;
        info.preset_version = ctx->presetVersion;
        info.warp_shader = gl != nullptr && gl->warpShaderActive ? 1 : 0;
        info.comp_shader = gl != nullptr && gl->compShaderActive ? 1 : 0;
        info.bass = ctx->mysound.imm_rel[0];
        info.mid = ctx->mysound.imm_rel[1];
        info.treb = ctx->mysound.imm_rel[2];
        info.bass_att = ctx->mysound.avg_rel[0];
        info.mid_att = ctx->mysound.avg_rel[1];
        info.treb_att = ctx->mysound.avg_rel[2];
        info.texsize_x = ctx->texsizeX;
        info.texsize_y = ctx->texsizeY;
        info.width = ctx->width;
        info.height = ctx->height;
        info.feedback_format = 0;
        info.gl_errors = gl != nullptr ? static_cast<int>(glErrors) : 0;
        info.limiter_active = gl != nullptr && gl->limiterEnabled ? 1 : 0;
        info.preset_name = ctx->presetName.c_str();
        info.last_error = ctx->lastError.c_str();
        info.warp_ps_version = presetInfo.warpPsVersion;
        info.comp_ps_version = presetInfo.compPsVersion;
        info.fallback_reason = presetInfo.fallbackReason;

        fn(&info, user);
    }

    void BdTickDebugInfo(sv_context* ctx)
    {
        const double now = BdWallSeconds();
        const double gap = now - ctx->debugLastCall;
        ctx->debugLastCall = now;

        if (ctx->debugWindowPending || gap > std::max(static_cast<double>(ctx->debugInterval), kBeatdropMaxFrameDtSeconds))
        {
            BdRestartDebugWindow(ctx, now);
            return;
        }

        ctx->debugWindowCalls++;

        const double elapsed = now - ctx->debugWindowStart;
        if (elapsed < static_cast<double>(ctx->debugInterval) || elapsed <= 0.0)
            return;

        BdEmitDebugInfo(ctx, elapsed);

        if (!ctx->debugWindowPending)
            BdRestartDebugWindow(ctx, now);
    }
}

extern "C" void sv_render_frame(sv_context* ctx, double dt_seconds, uint32_t target_fbo)
{
    if (ctx == nullptr)
        return;

    if (ctx->debugFn != nullptr && ctx->debugInterval > 0.0f)
        BdTickDebugInfo(ctx);

    if (ctx->paused)
    {
        if (!ctx->deterministic)
            BdGlPresentFrame(ctx, target_fbo);
        return;
    }

    ctx->lastFrameDt = dt_seconds;
    ctx->lastTargetFbo = target_fbo;

    BdConsumePresetLoadFailure(ctx);

    if (ctx->deterministic)
    {
        BdRenderLegacyFrame(ctx, target_fbo);
        return;
    }

    BdRenderFixedStepFrame(ctx, dt_seconds, target_fbo);
}
