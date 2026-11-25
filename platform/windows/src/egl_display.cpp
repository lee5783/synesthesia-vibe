//
//  egl_display.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#include "egl_display.h"

#include <synesthesia-vibe/synesthesia-vibe-windows.h>

#include <cstdio>
#include <mutex>

namespace
{
    constexpr int kAdapterCount = 2;

    struct SharedDisplay
    {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLConfig config = nullptr;
        int users = 0;
        uint64_t generation = 0;
    };

    std::mutex g_displayMutex;
    SharedDisplay g_displays[kAdapterCount];

    std::mutex g_glMutex;
    bool g_glLoaded = false;

    void* GetGlProcAddress(const char* name)
    {
        return reinterpret_cast<void*>(eglGetProcAddress(name));
    }

    bool InitializeDisplay(int adapter, SharedDisplay& shared, std::string& error)
    {
        const auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (getPlatformDisplay == nullptr)
        {
            error = "eglGetPlatformDisplayEXT is not available";
            return false;
        }

        const EGLint hardwareAttributes[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_NONE
        };
        const EGLint warpAttributes[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE,
            EGL_NONE
        };

        const EGLint* attributes = adapter == SVW_ADAPTER_WARP ? warpAttributes : hardwareAttributes;
        const EGLDisplay display = getPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, attributes);
        if (display == EGL_NO_DISPLAY)
        {
            error = svw::DescribeEglError("eglGetPlatformDisplayEXT (ANGLE Direct3D 11)", eglGetError());
            return false;
        }

        EGLint major = 0;
        EGLint minor = 0;
        if (eglInitialize(display, &major, &minor) != EGL_TRUE)
        {
            error = svw::DescribeEglError("eglInitialize (ANGLE Direct3D 11)", eglGetError());
            return false;
        }

        const EGLint configAttributes[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0,
            EGL_STENCIL_SIZE, 0,
            EGL_NONE
        };

        EGLConfig config = nullptr;
        EGLint configCount = 0;
        if (eglChooseConfig(display, configAttributes, &config, 1, &configCount) != EGL_TRUE || configCount < 1)
        {
            error = svw::DescribeEglError("eglChooseConfig (RGBA8, OpenGL ES 3)", eglGetError());
            eglTerminate(display);
            return false;
        }

        shared.display = display;
        shared.config = config;
        return true;
    }

    bool ValidAdapter(int adapter)
    {
        return adapter >= 0 && adapter < kAdapterCount;
    }
}

namespace svw
{

bool AcquireDisplay(int adapter, DisplayLease& outLease, std::string& error)
{
    if (!ValidAdapter(adapter))
    {
        error = "unknown adapter";
        return false;
    }

    std::lock_guard<std::mutex> lock(g_displayMutex);
    SharedDisplay& shared = g_displays[adapter];

    if (shared.display == EGL_NO_DISPLAY && !InitializeDisplay(adapter, shared, error))
        return false;

    ++shared.users;
    outLease.adapter = adapter;
    outLease.display = shared.display;
    outLease.config = shared.config;
    outLease.generation = shared.generation;
    return true;
}

void ReleaseDisplay(DisplayLease& lease)
{
    if (lease.display == EGL_NO_DISPLAY || !ValidAdapter(lease.adapter))
        return;

    std::lock_guard<std::mutex> lock(g_displayMutex);
    SharedDisplay& shared = g_displays[lease.adapter];

    if (shared.users > 0 && --shared.users == 0)
    {
        eglTerminate(shared.display);
        shared.display = EGL_NO_DISPLAY;
        shared.config = nullptr;
        ++shared.generation;
    }

    lease.display = EGL_NO_DISPLAY;
    lease.config = nullptr;
}

bool RecoverDisplay(DisplayLease& lease, std::string& error)
{
    if (!ValidAdapter(lease.adapter))
    {
        error = "unknown adapter";
        return false;
    }

    std::lock_guard<std::mutex> lock(g_displayMutex);
    SharedDisplay& shared = g_displays[lease.adapter];

    if (shared.generation == lease.generation)
    {
        if (shared.display != EGL_NO_DISPLAY)
            eglTerminate(shared.display);

        shared.display = EGL_NO_DISPLAY;
        shared.config = nullptr;
        ++shared.generation;

        if (!InitializeDisplay(lease.adapter, shared, error))
            return false;
    }

    if (shared.display == EGL_NO_DISPLAY)
    {
        error = "the shared EGL display could not be recovered";
        return false;
    }

    lease.display = shared.display;
    lease.config = shared.config;
    lease.generation = shared.generation;
    return true;
}

bool DisplayStale(const DisplayLease& lease)
{
    if (lease.display == EGL_NO_DISPLAY || !ValidAdapter(lease.adapter))
        return false;

    std::lock_guard<std::mutex> lock(g_displayMutex);
    return g_displays[lease.adapter].generation != lease.generation;
}

bool LoadGlFunctionsOnce(std::string& error)
{
    std::lock_guard<std::mutex> lock(g_glMutex);
    if (g_glLoaded)
        return true;

    const sv_status status = sv_init_gl(GetGlProcAddress);
    if (status != SV_OK)
    {
        error = "sv_init_gl failed with eglGetProcAddress";
        return false;
    }

    g_glLoaded = true;
    return true;
}

std::string DescribeEglError(const char* call, EGLint code)
{
    char message[160];
    snprintf(message, sizeof(message), "%s failed (EGL error 0x%04X)", call, static_cast<unsigned int>(code));
    return message;
}

}
