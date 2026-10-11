//
//  gl_object_counts.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "gl_object_counts.h"

#include <cstdio>

namespace render_gl
{

namespace
{

constexpr int kKindCount = static_cast<int>(GlObjectKind::Count);

const char* const kKindNames[kKindCount] = { "texture", "framebuffer", "program", "vertex_array", "buffer", "sampler" };

int g_created[kKindCount] = {};
int g_deleted[kKindCount] = {};

}

void NoteGlObjectCreated(GlObjectKind kind, int count)
{
    g_created[static_cast<int>(kind)] += count;
}

void NoteGlObjectDeleted(GlObjectKind kind, int count)
{
    g_deleted[static_cast<int>(kind)] += count;
}

int GlObjectsCreated()
{
    int total = 0;
    for (int i = 0; i < kKindCount; ++i)
        total += g_created[i];
    return total;
}

int GlObjectsDeleted()
{
    int total = 0;
    for (int i = 0; i < kKindCount; ++i)
        total += g_deleted[i];
    return total;
}

int GlObjectsLive()
{
    return GlObjectsCreated() - GlObjectsDeleted();
}

void LogGlObjectCounts(const char* szTag)
{
    const char* szSite = szTag != nullptr ? szTag : "(unnamed)";

    for (int i = 0; i < kKindCount; ++i)
        fprintf(stderr, "render_gl: %s gl_objects %s created=%d deleted=%d\n", szSite, kKindNames[i], g_created[i], g_deleted[i]);

    fprintf(stderr, "render_gl: %s gl_objects total created=%d deleted=%d live=%d %s\n", szSite,
        GlObjectsCreated(), GlObjectsDeleted(), GlObjectsLive(), GlObjectsLive() == 0 ? "BALANCED" : "LEAK");
}

}
