//
//  egl_display.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#ifndef __SYNESTHESIA_VIBE_WINDOWS_EGL_DISPLAY_H__
#define __SYNESTHESIA_VIBE_WINDOWS_EGL_DISPLAY_H__ 1

#include "windows_headers.h"

#include <cstdint>
#include <string>

namespace svw
{

struct DisplayLease
{
    int adapter = 0;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig config = nullptr;
    uint64_t generation = 0;
};

bool AcquireDisplay(int adapter, DisplayLease& outLease, std::string& error);
void ReleaseDisplay(DisplayLease& lease);
bool RecoverDisplay(DisplayLease& lease, std::string& error);
bool DisplayStale(const DisplayLease& lease);

bool LoadGlFunctionsOnce(std::string& error);

std::string DescribeEglError(const char* call, EGLint code);

}

#endif
