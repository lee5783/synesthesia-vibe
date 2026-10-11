//
//  gl_object_counts.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#ifndef __BEATDROP_RENDER_GL_GL_OBJECT_COUNTS_H__
#define __BEATDROP_RENDER_GL_GL_OBJECT_COUNTS_H__ 1

namespace render_gl
{

enum class GlObjectKind
{
    Texture = 0,
    Framebuffer,
    Program,
    VertexArray,
    Buffer,
    Sampler,
    Count
};

void NoteGlObjectCreated(GlObjectKind kind, int count = 1);

void NoteGlObjectDeleted(GlObjectKind kind, int count = 1);

int GlObjectsCreated();

int GlObjectsDeleted();

int GlObjectsLive();

void LogGlObjectCounts(const char* szTag);

}

#endif
