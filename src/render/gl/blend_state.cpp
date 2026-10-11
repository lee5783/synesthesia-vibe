//
//  blend_state.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "blend_state.h"
#include "gl_headers.h"

namespace render_gl
{

namespace
{

unsigned int ToGlFactor(BlendFactor factor)
{
    switch (factor)
    {
    case BlendFactor::Zero: return GL_ZERO;
    case BlendFactor::One: return GL_ONE;
    case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
    case BlendFactor::InvSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha: return GL_DST_ALPHA;
    case BlendFactor::InvDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
    case BlendFactor::SrcColor: return GL_SRC_COLOR;
    case BlendFactor::InvSrcColor: return GL_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DstColor: return GL_DST_COLOR;
    case BlendFactor::InvDstColor: return GL_ONE_MINUS_DST_COLOR;
    }
    return GL_ONE;
}

}

void SetBlendState(bool bEnable, BlendFactor src, BlendFactor dst)
{
    if (!bEnable)
    {
        glDisable(GL_BLEND);
        return;
    }

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(ToGlFactor(src), ToGlFactor(dst));
}

void SetBlendOpaque()
{
    SetBlendState(false, BlendFactor::One, BlendFactor::Zero);
}

void SetBlendAlpha()
{
    SetBlendState(true, BlendFactor::SrcAlpha, BlendFactor::InvSrcAlpha);
}

void SetBlendAlphaFlipped()
{
    SetBlendState(true, BlendFactor::InvSrcAlpha, BlendFactor::SrcAlpha);
}

void SetDefaultDrawState()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    SetBlendOpaque();
}

void SetBlendAdditive()
{
    SetBlendState(true, BlendFactor::One, BlendFactor::One);
}

void SetPointSize(float size)
{
#if !defined(SYNESTHESIA_VIBE_GLES)
    glPointSize(size);
#endif

    int program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);

    if (program == 0)
        return;

    const int location = glGetUniformLocation(static_cast<unsigned int>(program), "uPointSize");

    if (location >= 0)
        glUniform1f(location, size);
}

}
