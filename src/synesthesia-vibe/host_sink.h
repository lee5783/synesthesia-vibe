//
//  host_sink.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_HOST_SINK_H__
#define __SYNESTHESIA_VIBE_SYNESTHESIA_VIBE_HOST_SINK_H__ 1

#include <string>

#include "parser_host.h"
#include "time_model.h"

class BdHostSink : public ParserHostSink
{
public:
    BdHostSink();

    void SetTimeSource(const render_core::TimeModel* timeSource);

    void ReportError(wchar_t* message, float duration, int category, bool bold) override;
    void ShowUserMessage(const char* message, float duration, bool isError) override;
    float Now() override;

    void Clear();
    const std::string& LastError() const { return m_lastError; }
    int ErrorCount() const { return m_errorCount; }

private:
    void Record(const std::string& text);

    const render_core::TimeModel* m_timeSource;
    std::string m_lastError;
    int m_errorCount;
};

class BdRawClock : public render_core::IRawClock
{
public:
    BdRawClock();

    void SetDeterministic(bool deterministic);
    void SetTickDelta(long long tickDelta);
    void Advance();
    void Restart();

    bool QueryHighPrecisionTicks(long long& outTicks) override;
    unsigned QueryLowPrecisionMilliseconds() override;

private:
    bool m_deterministic;
    long long m_ticks;
    long long m_tickDelta;
};

#endif