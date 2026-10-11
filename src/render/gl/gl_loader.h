//
//  gl_loader.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#ifndef __BEATDROP_RENDER_GL_GL_LOADER_H__
#define __BEATDROP_RENDER_GL_GL_LOADER_H__ 1

namespace render_gl
{

typedef void* (*GlProcLoader)(const char* name);

bool LoadGlFunctions(GlProcLoader getProc);

}

#endif
