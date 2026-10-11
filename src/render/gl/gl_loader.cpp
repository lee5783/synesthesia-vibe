//
//  gl_loader.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#include "gl_loader.h"
#include "gl_error.h"
#include "gl_headers.h"

namespace render_gl
{

namespace
{
    GLADapiproc LoadThroughHost(void* userptr, const char* name)
    {
        const GlProcLoader getProc = *static_cast<const GlProcLoader*>(userptr);
        return reinterpret_cast<GLADapiproc>(getProc(name));
    }

    int LoadBackendTable(GlProcLoader getProc)
    {
#if defined(SYNESTHESIA_VIBE_GLES)
        return gladLoadGLES2UserPtr(LoadThroughHost, &getProc);
#else
        return gladLoadGLUserPtr(LoadThroughHost, &getProc);
#endif
    }
}

bool LoadGlFunctions(GlProcLoader getProc)
{
    if (getProc == nullptr)
        return false;

    if (LoadBackendTable(getProc) == 0)
        return false;

    return GlFunctionsLoaded();
}

}
