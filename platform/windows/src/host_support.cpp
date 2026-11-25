//
//  host_support.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-25.
//

#include "host_support.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
    constexpr int kGlesMajorVersion = 3;
    constexpr int kGlesMinorVersion = 0;
    constexpr const char* kRobustnessExtension = "EGL_EXT_create_context_robustness";

    thread_local bool t_hostRenderThread = false;
    std::mutex g_hostGlMutex;

    bool HasExtension(EGLDisplay display, const char* name)
    {
        const char* extensions = eglQueryString(display, EGL_EXTENSIONS);
        if (extensions == nullptr)
            return false;

        const size_t length = strlen(name);
        for (const char* found = strstr(extensions, name); found != nullptr; found = strstr(found + length, name))
        {
            const bool startsWord = found == extensions || found[-1] == ' ';
            const bool endsWord = found[length] == '\0' || found[length] == ' ';
            if (startsWord && endsWord)
                return true;
        }

        return false;
    }
}

namespace svw
{

std::mutex& HostGlMutex()
{
    return g_hostGlMutex;
}

void MarkHostRenderThread(bool marked)
{
    t_hostRenderThread = marked;
}

bool OnHostRenderThread()
{
    return t_hostRenderThread;
}

void WaitServicingSentMessages(HANDLE handle)
{
    for (;;)
    {
        const DWORD result = MsgWaitForMultipleObjectsEx(1, &handle, INFINITE, QS_SENDMESSAGE, 0);
        if (result == WAIT_OBJECT_0)
            return;

        if (result != WAIT_OBJECT_0 + 1)
        {
            WaitForSingleObject(handle, INFINITE);
            return;
        }

        MSG message;
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE | PM_QS_SENDMESSAGE);
    }
}

const char* UnpresentableReason(HWND hwnd)
{
    if (IsIconic(hwnd))
        return "the window is minimised";

    RECT client = {};
    if (!GetClientRect(hwnd, &client) || client.right <= client.left || client.bottom <= client.top)
        return "the window has no client area";

    return nullptr;
}

float ClampRenderScale(float scale)
{
    return std::min(kMaxRenderScale, std::max(kMinRenderScale, scale));
}

std::string ErrorText(sv_context* ctx, const char* fallback)
{
    const char* text = ctx != nullptr ? sv_last_error(ctx) : nullptr;
    return text != nullptr && text[0] != '\0' ? std::string(text) : std::string(fallback);
}

EGLContext CreateRenderContext(const DisplayLease& lease, std::string& error)
{
    std::vector<EGLint> contextAttributes = {
        EGL_CONTEXT_MAJOR_VERSION, kGlesMajorVersion,
        EGL_CONTEXT_MINOR_VERSION, kGlesMinorVersion
    };
    if (HasExtension(lease.display, kRobustnessExtension))
    {
        contextAttributes.push_back(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT);
        contextAttributes.push_back(EGL_LOSE_CONTEXT_ON_RESET_EXT);
    }
    contextAttributes.push_back(EGL_NONE);

    const EGLContext context = eglCreateContext(lease.display, lease.config, EGL_NO_CONTEXT, contextAttributes.data());
    if (context == EGL_NO_CONTEXT)
        error = DescribeEglError("eglCreateContext (OpenGL ES 3.0)", eglGetError());

    return context;
}

}
