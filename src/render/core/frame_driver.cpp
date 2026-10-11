//
//  frame_driver.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-01.
//

#include "frame_driver.h"

#include <cmath>

namespace render_core
{

FrameDriver::FrameDriver()
    : m_hardCutThresh(0)
{
}

void FrameDriver::Reset()
{
    m_hardCutThresh = 0;
}

void FrameDriver::Update(const FrameUpdateInput& input, float (&fWave)[2][576], FrameUpdateOutput& output)
{
    output = FrameUpdateOutput();

    output.isFirstFrame = (input.frame == 0);

    output.nextPresetTime = input.nextPresetTime;
    if (output.nextPresetTime < 0)
    {
        float dt = input.timeBetweenPresetsRand * input.nextPresetTimeRandomDraw;
        output.nextPresetTime = (float)input.time + input.blendTimeAuto + input.timeBetweenPresets + dt;
        output.nextPresetTimeWasInitialized = true;
    }

    if (!input.redraw)
    {
        output.presetTimerExpired = (output.nextPresetTime < input.time);

        if (input.isBlending)
        {
            output.blendProgressComputed = true;
            output.blendProgress = (float)((input.time - input.blendStartTime) / input.blendDuration);
            output.blendFinished = (output.blendProgress > 1.0f);
        }

        if (input.frame == 0)
            m_hardCutThresh = input.hardCutLoudnessThresh*2.0f;
        if (input.fps > 1.0f && !input.hardCutsDisabled && !input.presetLockedByUser && !input.presetLockedByCode)
        {
            if (input.immRel[0] + input.immRel[1] + input.immRel[2] > m_hardCutThresh*3.0f)
            {
                output.hardCutTriggered = true;
                m_hardCutThresh *= 2.0f;
            }
            else
            {
                float k = -1.3863f / (input.hardCutHalflife*input.fps);
                float single_frame_multiplier = expf(k);
                m_hardCutThresh = (m_hardCutThresh - input.hardCutLoudnessThresh)*single_frame_multiplier + input.hardCutLoudnessThresh;
            }
        }

        // smooth & scale the audio data, according to m_state, for display purposes
        float scale = input.waveScale / 128.0f;
        fWave[0][0] *= scale;
        fWave[1][0] *= scale;
        float mix2 = input.waveSmoothing;
        float mix1 = scale*(1.0f - mix2);
        for (int i=1; i<576; i++)
        {
            fWave[0][i] = fWave[0][i]*mix1 + fWave[0][i-1]*mix2;
            fWave[1][i] = fWave[1][i]*mix1 + fWave[1][i-1]*mix2;
        }
    }

    bool bOldPresetUsesWarpShader = (input.oldWarpPSVersion > 0);
    bool bNewPresetUsesWarpShader = (input.newWarpPSVersion > 0);
    bool bOldPresetUsesCompShader = (input.oldCompPSVersion > 0);
    bool bNewPresetUsesCompShader = (input.newCompPSVersion > 0);

    output.shaderBlendCode = (bOldPresetUsesWarpShader ? 8 : 0) |
                              (bOldPresetUsesCompShader ? 4 : 0) |
                              (bNewPresetUsesWarpShader ? 2 : 0) |
                              (bNewPresetUsesCompShader ? 1 : 0);
}

void FrameDriver::SampleDate(IDateSource& dateSource, DateInfo& outDate)
{
    dateSource.GetLocalDate(outDate);
}

void FrameDriver::SampleMousePosition(IMousePositionSource& mouseSource, float& outX, float& outY)
{
    mouseSource.GetMousePosition(outX, outY);
}

}
