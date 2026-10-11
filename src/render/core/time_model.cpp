//
//  time_model.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-01.
//

#include "time_model.h"

#include <cmath>

namespace render_core
{

TimeModel::TimeModel()
    : m_time(0)
    , m_fps(60)
    , m_lastRawTime(0)
    , m_highPrecisionFrequency(0)
    , m_timeHistPos(0)
    , m_resetOccurred(false)
{
    for (int i = 0; i < kTimeModelHistorySlots; ++i)
        m_timeHist[i] = 0;
}

void TimeModel::SetHighPrecisionFrequency(long long ticksPerSecond)
{
    m_highPrecisionFrequency = ticksPerSecond;
}

void TimeModel::Reset()
{
    m_time = 0;
    for (int i = 0; i < kTimeModelHistorySlots; ++i)
        m_timeHist[i] = 0;
    m_timeHistPos = 0;
}

void TimeModel::Step(int frame, IRawClock& clock, double timeVariableResetDelay, bool externalResetRequested)
{
    m_resetOccurred = false;

    if (frame==0)
    {
        m_fps = 60;
        m_time = 0;
        m_timeHistPos = 0;
    }

    double new_raw_time;
    float elapsed;

    if (m_highPrecisionFrequency != 0)
    {
        // get high-precision time
        // precision: usually from 1..6 us (MICROseconds), depending on the cpu speed.
        // (higher cpu speeds tend to have better precision here)
        long long t;
        if (!clock.QueryHighPrecisionTicks(t))
        {
            m_highPrecisionFrequency = 0;   // something went wrong (exception thrown) -> revert to crappy timer
        }
        else
        {
            new_raw_time = (double)t;
            elapsed = (float)((new_raw_time - m_lastRawTime)/(double)m_highPrecisionFrequency);
        }
    }

    if (m_highPrecisionFrequency == 0)
    {
        // get low-precision time
        // precision: usually 1 ms (MILLIsecond) for win98, and 10 ms for win2k.
        new_raw_time = (double)(clock.QueryLowPrecisionMilliseconds()*0.001);
        elapsed = (float)(new_raw_time - m_lastRawTime);
    }

    m_lastRawTime = new_raw_time;
    int slots_to_look_back = (m_highPrecisionFrequency==0) ? kTimeModelHistorySlots : kTimeModelHistorySlots/2;

    m_time += 1.0f/m_fps;
    if (m_time >= timeVariableResetDelay)
    {
        m_time = 0; // Reset the time variable after 250000 seconds.
        m_resetOccurred = true;
    }

    if (externalResetRequested)
    {
        m_time = 0;
        m_resetOccurred = true;
    }

    // timekeeping goals:
    //    1. keep 'm_time' increasing SMOOTHLY: (smooth animation depends on it)
    //          m_time += 1.0f/m_fps;     // where m_fps is a bit damped
    //    2. keep m_time_hist[] 100% accurate (except for filtering out pauses),
    //       so that when we look take the difference between two entries,
    //       we get the real amount of time that passed between those 2 frames.
    //          m_time_hist[i] = m_last_raw_time + elapsed_corrected;

    if (frame > kTimeModelHistorySlots)
    {
        if (m_fps < 60.0f)
            slots_to_look_back = (int)(slots_to_look_back*(0.1f + 0.9f*(m_fps/60.0f)));

        if (elapsed > 5.0f/m_fps || elapsed > 1.0f || elapsed < 0)
            elapsed = 1.0f / 30.0f;

        float old_hist_time = m_timeHist[(m_timeHistPos - slots_to_look_back + kTimeModelHistorySlots) % kTimeModelHistorySlots];
        float new_hist_time = m_timeHist[(m_timeHistPos - 1 + kTimeModelHistorySlots) % kTimeModelHistorySlots]
                              + elapsed;

        m_timeHist[m_timeHistPos] = new_hist_time;
        m_timeHistPos = (m_timeHistPos+1) % kTimeModelHistorySlots;

        float new_fps = slots_to_look_back / (float)(new_hist_time - old_hist_time);
        float damping = (m_highPrecisionFrequency==0) ? 0.93f : 0.87f;

        // damp heavily, so that crappy timer precision doesn't make animation jerky
        if (fabsf(m_fps - new_fps) > 3.0f)
            m_fps = new_fps;
        else
            m_fps = damping*m_fps + (1-damping)*new_fps;
    }
    else
    {
        float damping = (m_highPrecisionFrequency==0) ? 0.8f : 0.6f;

        if (frame < 2)
            elapsed = 1.0f / 30.0f;
        else if (elapsed > 1.0f || elapsed < 0)
            elapsed = 1.0f / m_fps;

        float old_hist_time = m_timeHist[0];
        float new_hist_time = m_timeHist[(m_timeHistPos - 1 + kTimeModelHistorySlots) % kTimeModelHistorySlots]
                              + elapsed;

        m_timeHist[m_timeHistPos] = new_hist_time;
        m_timeHistPos = (m_timeHistPos+1) % kTimeModelHistorySlots;

        if (frame > 0)
        {
            float new_fps = (frame) / (new_hist_time - old_hist_time);
            m_fps = damping*m_fps + (1-damping)*new_fps;
        }
    }
}

}
