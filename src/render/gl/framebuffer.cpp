//
//  framebuffer.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "framebuffer.h"
#include "gl_error.h"
#include "gl_object_counts.h"
#include "gl_headers.h"

#include <cstdio>

namespace render_gl
{

Framebuffer::Framebuffer()
    : m_id(0)
    , m_width(0)
    , m_height(0)
{
}

Framebuffer::~Framebuffer()
{
    Destroy();
}

bool Framebuffer::Create(int width, int height)
{
    Destroy();

    if (width <= 0 || height <= 0)
        return false;

    if (!m_color.Create(width, height))
        return false;

    glGenFramebuffers(1, &m_id);
    NoteGlObjectCreated(GlObjectKind::Framebuffer);

    if (!CheckGlError("Framebuffer::Create glGenFramebuffers"))
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, m_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color.Id(), 0);

    const unsigned int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "render_gl: Framebuffer::Create incomplete FBO status 0x%04X at %dx%d\n",
            status, width, height);
        Destroy();
        return false;
    }

    if (!CheckGlError("Framebuffer::Create attach"))
    {
        Destroy();
        return false;
    }

    m_width = width;
    m_height = height;
    return true;
}

void Framebuffer::Destroy()
{
    if (m_id != 0)
    {
        glDeleteFramebuffers(1, &m_id);
        NoteGlObjectDeleted(GlObjectKind::Framebuffer);
        m_id = 0;
    }

    m_color.Destroy();
    m_width = 0;
    m_height = 0;
}

bool Framebuffer::Resize(int width, int height)
{
    if (m_id != 0 && width == m_width && height == m_height)
        return true;

    return Create(width, height);
}

void Framebuffer::Bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_id);
    glViewport(0, 0, m_width, m_height);
}

void Framebuffer::Unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}