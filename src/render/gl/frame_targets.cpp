//
//  frame_targets.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "frame_targets.h"
#include "gl_error.h"
#include "gl_headers.h"

namespace render_gl
{

namespace
{

void ClearFramebuffer(const Framebuffer& target)
{
    target.Bind();
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

}

void BindExternalTarget(unsigned int fbo, int width, int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

FrameTargets::FrameTargets()
    : m_sourceIndex(0)
{
}

bool FrameTargets::Create(int width, int height)
{
    Destroy();

    if (!m_vs[0].Create(width, height) || !m_vs[1].Create(width, height))
    {
        Destroy();
        return false;
    }

    ClearAll();
    return CheckGlError("FrameTargets::Create");
}

void FrameTargets::Destroy()
{
    m_vs[0].Destroy();
    m_vs[1].Destroy();
    m_sourceIndex = 0;
}

bool FrameTargets::Resize(int width, int height)
{
    if (Valid() && width == Width() && height == Height())
        return true;

    return Create(width, height);
}

void FrameTargets::Swap()
{
    m_sourceIndex = 1 - m_sourceIndex;
}

void FrameTargets::ClearSource()
{
    ClearFramebuffer(Source());
}

void FrameTargets::ClearAll()
{
    ClearFramebuffer(m_vs[0]);
    ClearFramebuffer(m_vs[1]);
}

}
