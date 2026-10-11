//
//  frame_driver.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-01.
//

#ifndef __BEATDROP_RENDER_CORE_FRAME_DRIVER_H__
#define __BEATDROP_RENDER_CORE_FRAME_DRIVER_H__ 1

namespace render_core
{

struct DateInfo
{
    float year;
    float month;
    float day;
    float weekday;
    float hour;
    float minute;
    float second;
    float milliseconds;
    float totalSeconds;
};

class IDateSource
{
public:
    virtual ~IDateSource() {}

    virtual void GetLocalDate(DateInfo& outDate) = 0;
};

class IMousePositionSource
{
public:
    virtual ~IMousePositionSource() {}

    virtual void GetMousePosition(float& outX, float& outY) = 0;
};

struct FrameUpdateInput
{
    int frame;
    bool redraw;
    double time;
    float fps;

    float nextPresetTime;
    float nextPresetTimeRandomDraw;
    float timeBetweenPresetsRand;
    float blendTimeAuto;
    float timeBetweenPresets;

    bool isBlending;
    float blendStartTime;
    float blendDuration;

    bool hardCutsDisabled;
    bool presetLockedByUser;
    bool presetLockedByCode;
    float hardCutLoudnessThresh;
    float hardCutHalflife;
    float immRel[3];

    float waveScale;
    float waveSmoothing;

    int oldWarpPSVersion;
    int newWarpPSVersion;
    int oldCompPSVersion;
    int newCompPSVersion;
};

struct FrameUpdateOutput
{
    bool isFirstFrame;

    float nextPresetTime;
    bool nextPresetTimeWasInitialized;
    bool presetTimerExpired;

    bool blendProgressComputed;
    float blendProgress;
    bool blendFinished;

    bool hardCutTriggered;

    int shaderBlendCode;
};

class FrameDriver
{
public:
    FrameDriver();

    void Reset();

    void Update(const FrameUpdateInput& input, float (&fWave)[2][576], FrameUpdateOutput& output);

    void SampleDate(IDateSource& dateSource, DateInfo& outDate);
    void SampleMousePosition(IMousePositionSource& mouseSource, float& outX, float& outY);

private:
    float m_hardCutThresh;
};

}

#endif
