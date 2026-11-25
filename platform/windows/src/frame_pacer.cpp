//
//  frame_pacer.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#include "frame_pacer.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <vector>

namespace
{
    constexpr double kFallbackRefreshHz = 60.0;
    constexpr double kRefreshRequerySeconds = 2.0;
    constexpr double kHundredNanosecondsPerSecond = 10000000.0;

    double ExactRefreshHz(const wchar_t* gdiDeviceName)
    {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
            return 0.0;

        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
            return 0.0;

        for (UINT32 index = 0; index < pathCount; ++index)
        {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source = {};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = sizeof(source);
            source.header.adapterId = paths[index].sourceInfo.adapterId;
            source.header.id = paths[index].sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS || wcscmp(source.viewGdiDeviceName, gdiDeviceName) != 0)
                continue;

            const DISPLAYCONFIG_RATIONAL rate = paths[index].targetInfo.refreshRate;
            if (rate.Numerator != 0 && rate.Denominator != 0)
                return static_cast<double>(rate.Numerator) / static_cast<double>(rate.Denominator);
        }

        return 0.0;
    }
}

namespace svw
{

FramePacer::FramePacer()
    : m_timer(nullptr)
    , m_ticksPerSecond(1.0)
    , m_monitor(nullptr)
    , m_refreshHz(kFallbackRefreshHz)
    , m_refreshQueriedAt(-kRefreshRequerySeconds)
    , m_preferredFps(0.0)
{
    LARGE_INTEGER frequency;
    if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0)
        m_ticksPerSecond = static_cast<double>(frequency.QuadPart);

    m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (m_timer == nullptr)
        m_timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
}

FramePacer::~FramePacer()
{
    if (m_timer != nullptr)
        CloseHandle(m_timer);
}

double FramePacer::Now() const
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / m_ticksPerSecond;
}

void FramePacer::Arm(double seconds)
{
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -std::max<LONGLONG>(1, static_cast<LONGLONG>(seconds * kHundredNanosecondsPerSecond));
    SetWaitableTimerEx(m_timer, &dueTime, 0, nullptr, nullptr, nullptr, 0);
}

bool FramePacer::RefreshDue(HWND hwnd, double now) const
{
    return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) != m_monitor || now - m_refreshQueriedAt >= kRefreshRequerySeconds;
}

bool FramePacer::RefreshDue(const std::vector<HWND>& windows, double now) const
{
    if (now - m_refreshQueriedAt >= kRefreshRequerySeconds || windows.size() != m_monitors.size())
        return true;

    for (size_t index = 0; index < windows.size(); ++index)
    {
        if (MonitorFromWindow(windows[index], MONITOR_DEFAULTTONEAREST) != m_monitors[index])
            return true;
    }

    return false;
}

bool FramePacer::TrackMonitor(HWND hwnd, double now)
{
    const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == m_monitor && now - m_refreshQueriedAt < kRefreshRequerySeconds)
        return false;

    const double previousRefreshHz = m_refreshHz;
    m_monitor = monitor;
    m_refreshHz = QueryRefreshHz(monitor);
    m_refreshQueriedAt = now;
    return m_refreshHz != previousRefreshHz;
}

bool FramePacer::TrackMonitors(const std::vector<HWND>& windows, double now)
{
    std::vector<HMONITOR> monitors;
    monitors.reserve(windows.size());
    for (HWND window : windows)
        monitors.push_back(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST));

    if (monitors == m_monitors && now - m_refreshQueriedAt < kRefreshRequerySeconds)
        return false;

    double highestHz = 0.0;
    for (HMONITOR monitor : monitors)
        highestHz = std::max(highestHz, QueryRefreshHz(monitor));

    const double previousRefreshHz = m_refreshHz;
    m_monitors = monitors;
    m_refreshHz = highestHz > 0.0 ? highestHz : kFallbackRefreshHz;
    m_refreshQueriedAt = now;
    return m_refreshHz != previousRefreshHz;
}

void FramePacer::SetPreferredFps(double fps)
{
    m_preferredFps = std::isfinite(fps) && fps > 0.0 ? fps : 0.0;
}

double FramePacer::EffectiveFps() const
{
    if (m_preferredFps <= 0.0)
        return m_refreshHz;

    return std::min(m_preferredFps, m_refreshHz);
}

double FramePacer::PeriodSeconds() const
{
    return 1.0 / EffectiveFps();
}

double FramePacer::QueryRefreshHz(HMONITOR monitor) const
{
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &info))
        return kFallbackRefreshHz;

    const double exactHz = ExactRefreshHz(info.szDevice);
    if (exactHz > 1.0)
        return exactHz;

    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) || mode.dmDisplayFrequency <= 1)
        return kFallbackRefreshHz;

    return static_cast<double>(mode.dmDisplayFrequency);
}

}
