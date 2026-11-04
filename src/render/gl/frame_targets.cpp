//
//  frame_targets.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "frame_targets.h"
#include "blend_state.h"
#include "gl_error.h"
#include "gl_headers.h"
#include "mesh.h"

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

void CopyColor(const DrawProgram& program, MeshBuffer& quadMesh, const Texture& source, const Framebuffer& target)
{
    render_core::Vertex quad[4] = {};
    const float corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f } };

    for (int i = 0; i < 4; i++)
    {
        quad[i].x = corners[i][0];
        quad[i].y = corners[i][1];
        quad[i].Diffuse = 0xFFFFFFFFu;
        quad[i].tu = corners[i][0] * 0.5f + 0.5f;
        quad[i].tv = corners[i][1] * 0.5f + 0.5f;
    }

    target.Bind();
    SetBlendOpaque();

    program.Bind();
    program.SetUvOffset(0.0f, 0.0f);
    program.SetPositionOffset(0.0f, 0.0f);

    glActiveTexture(GL_TEXTURE0);
    source.Bind();

    quadMesh.UploadVertices(quad, 4);
    quadMesh.DrawTriangleStrip(4);
}

}

void BindExternalTarget(unsigned int fbo, int width, int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
}

FrameTargets::FrameTargets()
    : m_sourceIndex(0)
    , m_format(TextureFormat::Rgba8)
{
}

bool FrameTargets::Create(int width, int height, TextureFormat format)
{
    Destroy();

    if (!m_vs[0].Create(width, height, format) || !m_vs[1].Create(width, height, format))
    {
        Destroy();
        return false;
    }

    m_format = format;
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

    return Create(width, height, m_format);
}

bool FrameTargets::ChangeFormat(TextureFormat format, const DrawProgram& program, MeshBuffer& quadMesh)
{
    if (!Valid())
        return false;

    if (format == m_format)
        return true;

    const TextureFormat previous = m_format;
    const int width = Width();
    const int height = Height();

    if (!Dest().SetColorFormat(format))
    {
        if (Dest().SetColorFormat(previous))
            CopyColor(program, quadMesh, Source().ColorTexture(), Dest());
        else
            Create(width, height, previous);

        CheckGlError("FrameTargets::ChangeFormat restore");
        return false;
    }

    CopyColor(program, quadMesh, Source().ColorTexture(), Dest());

    if (!Source().SetColorFormat(format))
    {
        const bool sourceRestored = Source().SetColorFormat(previous);

        if (sourceRestored)
            CopyColor(program, quadMesh, Dest().ColorTexture(), Source());

        if (sourceRestored && Dest().SetColorFormat(previous))
            CopyColor(program, quadMesh, Source().ColorTexture(), Dest());
        else
            Create(width, height, previous);

        CheckGlError("FrameTargets::ChangeFormat restore");
        return false;
    }

    CopyColor(program, quadMesh, Dest().ColorTexture(), Source());

    m_format = format;
    return CheckGlError("FrameTargets::ChangeFormat");
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
