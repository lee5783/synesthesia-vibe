//
//  audio_input.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_AUDIO_INPUT_H__
#define __SYNESTHESIA_VIBE_WINDOWS_AUDIO_INPUT_H__ 1

#include <synesthesia-vibe/synesthesia-vibe-windows.h>

#include "windows_headers.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct IMMDeviceEnumerator;

struct svw_audio_input
{
public:
    explicit svw_audio_input(int source);
    ~svw_audio_input();

    svw_audio_input(const svw_audio_input&) = delete;
    svw_audio_input& operator=(const svw_audio_input&) = delete;

    sv_status Start();
    sv_status Stop();
    sv_status SetGain(float gain);
    sv_status SetPcmCallback(svw_pcm_fn fn, void* user);

    std::string LastError() const;
    std::string DeviceName() const;
    sv_status Fail(sv_status status, const std::string& message);
    bool OnCaptureThread() const { return m_captureThreadId.load() == GetCurrentThreadId(); }

private:
    struct Stream;

    void ThreadMain();
    void CaptureLoop(IMMDeviceEnumerator* enumerator, HANDLE deviceChanged, Stream& stream);
    sv_status OpenStream(IMMDeviceEnumerator* enumerator, Stream& stream, std::string& error);
    void CloseStream(Stream& stream);
    bool DrainPackets(Stream& stream);
    void ConvertPacket(Stream& stream, const BYTE* data, UINT32 frames, bool silent);
    void PushStaged();
    void PushSilence(double seconds);
    void SetDeviceName(const std::string& name);

    int m_source;
    std::atomic<float> m_gain;

    std::mutex m_controlMutex;
    std::thread m_thread;
    std::atomic<DWORD> m_captureThreadId;
    HANDLE m_stop;
    HANDLE m_started;
    sv_status m_startStatus;
    std::string m_startError;
    bool m_running;

    std::mutex m_callbackMutex;
    svw_pcm_fn m_pcmFn;
    void* m_pcmUser;

    mutable std::mutex m_errorMutex;
    std::string m_lastError;
    std::string m_deviceName;

    std::vector<float> m_staged;
    double m_lastPushAt;
    double m_ticksPerSecond;
};

#endif
