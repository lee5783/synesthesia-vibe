//
//  host_support.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_HOST_SUPPORT_H__
#define __SYNESTHESIA_VIBE_WINDOWS_HOST_SUPPORT_H__ 1

#include <synesthesia-vibe/synesthesia-vibe.h>

#include "egl_display.h"

#include <cstddef>
#include <mutex>
#include <string>

namespace svw
{

constexpr float kMinRenderScale = 0.25f;
constexpr float kMaxRenderScale = 1.0f;
constexpr size_t kCoreStackHeadroomBytes = 16384;
constexpr const char* kRendererUnavailablePrefix = "renderer unavailable";

std::mutex& HostGlMutex();
void MarkHostRenderThread(bool marked);
bool OnHostRenderThread();
void WaitServicingSentMessages(HANDLE handle);
const char* UnpresentableReason(HWND hwnd);
float ClampRenderScale(float scale);
std::string ErrorText(sv_context* ctx, const char* fallback);
EGLContext CreateRenderContext(const DisplayLease& lease, std::string& error);

class ScopedEvent
{
public:
    ScopedEvent() : m_handle(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}
    ~ScopedEvent()
    {
        if (m_handle != nullptr)
            CloseHandle(m_handle);
    }

    ScopedEvent(const ScopedEvent&) = delete;
    ScopedEvent& operator=(const ScopedEvent&) = delete;

    HANDLE Get() const { return m_handle; }

private:
    HANDLE m_handle;
};

}

#endif
