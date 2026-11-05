//
//  time_model.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-01.
//

#ifndef __BEATDROP_RENDER_CORE_TIME_MODEL_H__
#define __BEATDROP_RENDER_CORE_TIME_MODEL_H__ 1

namespace render_core
{

constexpr int kTimeModelHistorySlots = 128;

class IRawClock
{
public:
    virtual ~IRawClock() {}

    virtual bool QueryHighPrecisionTicks(long long& outTicks) = 0;

    virtual unsigned QueryLowPrecisionMilliseconds() = 0;
};

class TimeModel
{
public:
    TimeModel();

    void SetHighPrecisionFrequency(long long ticksPerSecond);
    long long GetHighPrecisionFrequency() const { return m_highPrecisionFrequency; }

    void Reset();

    void Step(int frame, IRawClock& clock, double timeVariableResetDelay, bool externalResetRequested);

    double GetTime() const { return m_time; }
    float GetFps() const { return m_fps; }
    double GetLastRawTime() const { return m_lastRawTime; }
    bool TimeWasReset() const { return m_resetOccurred; }

private:
    double m_time;
    float m_fps;
    double m_lastRawTime;
    long long m_highPrecisionFrequency;
    float m_timeHist[kTimeModelHistorySlots];
    int m_timeHistPos;
    bool m_resetOccurred;
};

}

#endif
