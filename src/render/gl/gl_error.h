//
//  gl_error.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __BEATDROP_RENDER_GL_GL_ERROR_H__
#define __BEATDROP_RENDER_GL_GL_ERROR_H__ 1

namespace render_gl
{

constexpr int kMaxDrainedErrors = 64;

const char* GlErrorName(unsigned int error);

int DrainGlErrors(const char* szCallSite);

bool CheckGlError(const char* szCallSite);

bool GlFunctionsLoaded();

}

#endif