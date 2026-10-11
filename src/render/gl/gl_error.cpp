//
//  gl_error.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "gl_error.h"
#include "gl_headers.h"

#include <cstdio>

namespace render_gl
{

const char* GlErrorName(unsigned int error)
{
    switch (error)
    {
    case GL_NO_ERROR: return "GL_NO_ERROR";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    default: break;
    }
    return "GL_UNKNOWN_ERROR";
}

int DrainGlErrors(const char* szCallSite)
{
    const char* szSite = szCallSite != nullptr ? szCallSite : "(unnamed)";
    int count = 0;

    for (int i = 0; i < kMaxDrainedErrors; ++i)
    {
        const unsigned int error = glGetError();
        if (error == GL_NO_ERROR)
            break;

        ++count;
        fprintf(stderr, "render_gl: GL error 0x%04X (%s) at %s\n", error, GlErrorName(error), szSite);
    }

    return count;
}

bool CheckGlError(const char* szCallSite)
{
    return DrainGlErrors(szCallSite) == 0;
}

bool GlFunctionsLoaded()
{
    return glGenTextures != nullptr && glDrawElements != nullptr;
}

}
