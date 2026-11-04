//
//  frame_interpolator.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-04.
//

#include "frame_interpolator.h"
#include "frame_targets.h"
#include "gl_error.h"
#include "gl_headers.h"
#include "shader_prologue.h"

#include <cstdio>

namespace render_gl
{

namespace
{

const char* const kInterpolatorVertexSource =
    "void main()\n"
    "{\n"
    "    vec2 pos[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));\n"
    "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"
    "}\n";

const char* const kInterpolatorFragmentSource =
    "uniform sampler2D uPrevious;\n"
    "uniform sampler2D uNewest;\n"
    "uniform float uAlpha;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    ivec2 texel = ivec2(gl_FragCoord.xy);\n"
    "    vec4 previous = texelFetch(uPrevious, texel, 0);\n"
    "    vec4 newest = texelFetch(uNewest, texel, 0);\n"
    "    fragColor = mix(previous, newest, uAlpha);\n"
    "}\n";

}

FrameInterpolator::FrameInterpolator()
    : m_previousLocation(-1)
    , m_newestLocation(-1)
    , m_alphaLocation(-1)
    , m_newestIndex(0)
    , m_capturedCount(0)
{
}

bool FrameInterpolator::Create(int width, int height)
{
    Destroy();

    const ShaderBuildResult result = m_blend.BuildFromSource(WithShaderPrologue(ShaderStage::Vertex, kInterpolatorVertexSource).c_str(),
        WithShaderPrologue(ShaderStage::Fragment, kInterpolatorFragmentSource).c_str());

    if (!result.ok)
    {
        fprintf(stderr, "render_gl: FrameInterpolator blend pass build failed: %s\n", result.log.c_str());
        Destroy();
        return false;
    }

    const unsigned int program = m_blend.Id();
    m_previousLocation = glGetUniformLocation(program, "uPrevious");
    m_newestLocation = glGetUniformLocation(program, "uNewest");
    m_alphaLocation = glGetUniformLocation(program, "uAlpha");

    if (!m_emptyArray.Create() || !m_slots[0].Create(width, height) || !m_slots[1].Create(width, height))
    {
        fprintf(stderr, "render_gl: FrameInterpolator targets create failed at %dx%d\n", width, height);
        Destroy();
        return false;
    }

    ResetHistory();
    return CheckGlError("FrameInterpolator::Create");
}

void FrameInterpolator::Destroy()
{
    m_slots[0].Destroy();
    m_slots[1].Destroy();
    m_emptyArray.Destroy();
    m_blend.Destroy();
    m_previousLocation = m_newestLocation = m_alphaLocation = -1;
    ResetHistory();
}

void FrameInterpolator::ResetHistory()
{
    m_newestIndex = 0;
    m_capturedCount = 0;
}

void FrameInterpolator::CommitCapture()
{
    m_newestIndex = 1 - m_newestIndex;

    if (m_capturedCount < 2)
        m_capturedCount++;
}

void FrameInterpolator::Blend(float alpha, unsigned int targetFbo)
{
    if (!Valid() || !HasHistory())
        return;

    const Framebuffer& newest = m_slots[m_newestIndex];
    const Framebuffer& previous = HasPrevious() ? m_slots[1 - m_newestIndex] : newest;

    glDisable(GL_BLEND);
    BindExternalTarget(targetFbo, newest.Width(), newest.Height());
    m_emptyArray.Bind();
    m_blend.Bind();

    if (m_alphaLocation >= 0)
        glUniform1f(m_alphaLocation, HasPrevious() ? alpha : 1.0f);

    glActiveTexture(GL_TEXTURE0);
    previous.ColorTexture().Bind();

    if (m_previousLocation >= 0)
        glUniform1i(m_previousLocation, 0);

    glActiveTexture(GL_TEXTURE1);
    newest.ColorTexture().Bind();

    if (m_newestLocation >= 0)
        glUniform1i(m_newestLocation, 1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    for (int unit = 1; unit >= 0; unit--)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(0);
}

}
