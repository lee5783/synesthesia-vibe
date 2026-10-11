//
//  host_sink.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "host_sink.h"

#include <chrono>

#include "context.h"

BdHostSink::BdHostSink()
    : m_timeSource(nullptr)
    , m_errorCount(0)
{
}

void BdHostSink::SetTimeSource(const render_core::TimeModel* timeSource)
{
    m_timeSource = timeSource;
}

void BdHostSink::Clear()
{
    m_lastError.clear();
    m_errorCount = 0;
}

void BdHostSink::Record(const std::string& text)
{
    if (!text.empty())
    {
        m_lastError = text;
        m_errorCount++;
    }
}

void BdHostSink::ReportError(wchar_t* message, float duration, int category, bool bold)
{
    (void)duration;
    (void)bold;

    if (message == nullptr || message[0] == L'\0')
        return;

    std::string text = BdWideToUtf8(message);
    if (category == ERR_PRESET || category == ERR_INIT)
        text += " [init]";
    else if (category == ERR_SHADER_PRECACHE)
        text += " [shader]";

    Record(text);
}

void BdHostSink::ShowUserMessage(const char* message, float duration, bool isError)
{
    (void)duration;

    if (!isError || message == nullptr || message[0] == '\0')
        return;

    Record(std::string(message));
}

float BdHostSink::Now()
{
    if (m_timeSource != nullptr)
        return static_cast<float>(m_timeSource->GetTime());

    return 0.0f;
}

BdRawClock::BdRawClock()
    : m_deterministic(false)
    , m_ticks(0)
    , m_tickDelta(0)
{
}

void BdRawClock::SetDeterministic(bool deterministic)
{
    m_deterministic = deterministic;
}

void BdRawClock::SetTickDelta(long long tickDelta)
{
    m_tickDelta = tickDelta;
}

void BdRawClock::Advance()
{
    if (m_deterministic)
        m_ticks += m_tickDelta;
    else
        m_ticks = static_cast<long long>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 100);
}

void BdRawClock::Restart()
{
    m_ticks = 0;
}

bool BdRawClock::QueryHighPrecisionTicks(long long& outTicks)
{
    outTicks = m_ticks;
    return true;
}

unsigned BdRawClock::QueryLowPrecisionMilliseconds()
{
    return static_cast<unsigned>(m_ticks / 10000);
}