//
//  frame_targets.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#ifndef __BEATDROP_RENDER_GL_FRAME_TARGETS_H__
#define __BEATDROP_RENDER_GL_FRAME_TARGETS_H__ 1

#include "framebuffer.h"

namespace render_gl
{

void BindExternalTarget(unsigned int fbo, int width, int height);

class FrameTargets
{
public:
    FrameTargets();

    bool Create(int width, int height);
    void Destroy();
    bool Resize(int width, int height);

    void Swap();
    void ClearSource();
    void ClearAll();

    Framebuffer& Source() { return m_vs[m_sourceIndex]; }
    const Framebuffer& Source() const { return m_vs[m_sourceIndex]; }
    Framebuffer& Dest() { return m_vs[1 - m_sourceIndex]; }
    const Framebuffer& Dest() const { return m_vs[1 - m_sourceIndex]; }

    int Width() const { return m_vs[0].Width(); }
    int Height() const { return m_vs[0].Height(); }
    bool Valid() const { return m_vs[0].Valid() && m_vs[1].Valid(); }

private:
    Framebuffer m_vs[2];
    int m_sourceIndex;
};

}

#endif
