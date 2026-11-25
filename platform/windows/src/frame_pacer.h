//
//  frame_pacer.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_FRAME_PACER_H__
#define __SYNESTHESIA_VIBE_WINDOWS_FRAME_PACER_H__ 1

#include "windows_headers.h"

#include <vector>

namespace svw
{

class FramePacer
{
public:
    FramePacer();
    ~FramePacer();

    FramePacer(const FramePacer&) = delete;
    FramePacer& operator=(const FramePacer&) = delete;

    bool Valid() const { return m_timer != nullptr; }
    HANDLE Timer() const { return m_timer; }

    double Now() const;
    void Arm(double seconds);

    bool RefreshDue(HWND hwnd, double now) const;
    bool RefreshDue(const std::vector<HWND>& windows, double now) const;
    bool TrackMonitor(HWND hwnd, double now);
    bool TrackMonitors(const std::vector<HWND>& windows, double now);
    void SetPreferredFps(double fps);

    double RefreshHz() const { return m_refreshHz; }
    double EffectiveFps() const;
    double PeriodSeconds() const;

private:
    double QueryRefreshHz(HMONITOR monitor) const;

    HANDLE m_timer;
    double m_ticksPerSecond;
    HMONITOR m_monitor;
    std::vector<HMONITOR> m_monitors;
    double m_refreshHz;
    double m_refreshQueriedAt;
    double m_preferredFps;
};

}

#endif
