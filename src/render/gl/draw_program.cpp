//
//  draw_program.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#include "draw_program.h"
#include "gl_error.h"
#include "gl_headers.h"
#include "shader_prologue.h"

#include <cstdio>

namespace render_gl
{

namespace
{

const char* const kDrawVertexSource =
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "layout(location = 2) in vec2 aUv;\n"
    "uniform vec2 uUvOffset;\n"
    "uniform vec2 uPosOffset;\n"
    "uniform float uPointSize;\n"
    "out vec4 vColor;\n"
    "out vec2 vUv;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPosition.x + uPosOffset.x, -aPosition.y + uPosOffset.y, aPosition.z, 1.0);\n"
    "    vColor = SV_COLOR_SWIZZLE(aColor);\n"
    "    vec2 uv = aUv + uUvOffset;\n"
    "    vUv = vec2(uv.x, 1.0 - uv.y);\n"
    "    gl_PointSize = uPointSize;\n"
    "}\n";

const char* const kTexturedFragmentSource =
    "uniform sampler2D uTexture;\n"
    "in vec4 vColor;\n"
    "in vec2 vUv;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    vec4 texel = texture(uTexture, vUv);\n"
    "    fragColor = vec4(texel.rgb * vColor.rgb, vColor.a);\n"
    "}\n";

const char* const kUntexturedFragmentSource =
    "in vec4 vColor;\n"
    "in vec2 vUv;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    fragColor = vColor;\n"
    "}\n";

}

DrawProgram::DrawProgram()
    : m_bTextured(false)
    , m_uvOffsetLocation(-1)
    , m_posOffsetLocation(-1)
    , m_textureLocation(-1)
    , m_pointSizeLocation(-1)
{
}

bool DrawProgram::Build(bool bTextured)
{
    Destroy();

    const ShaderBuildResult result = m_shader.BuildFromSource(WithShaderPrologue(ShaderStage::Vertex, kDrawVertexSource).c_str(),
        WithShaderPrologue(ShaderStage::Fragment, bTextured ? kTexturedFragmentSource : kUntexturedFragmentSource).c_str());

    if (!result.ok)
    {
        fprintf(stderr, "render_gl: DrawProgram::Build(%s) failed: %s\n", bTextured ? "textured" : "untextured", result.log.c_str());
        return false;
    }

    m_bTextured = bTextured;
    m_uvOffsetLocation = glGetUniformLocation(m_shader.Id(), "uUvOffset");
    m_posOffsetLocation = glGetUniformLocation(m_shader.Id(), "uPosOffset");
    m_textureLocation = glGetUniformLocation(m_shader.Id(), "uTexture");
    m_pointSizeLocation = glGetUniformLocation(m_shader.Id(), "uPointSize");

    m_shader.Bind();

    if (m_textureLocation >= 0)
        glUniform1i(m_textureLocation, 0);

    if (m_uvOffsetLocation >= 0)
        glUniform2f(m_uvOffsetLocation, 0.0f, 0.0f);

    if (m_posOffsetLocation >= 0)
        glUniform2f(m_posOffsetLocation, 0.0f, 0.0f);

    SetPointSize(1.0f);

    return CheckGlError("DrawProgram::Build");
}

void DrawProgram::Destroy()
{
    m_shader.Destroy();
    m_bTextured = false;
    m_uvOffsetLocation = -1;
    m_posOffsetLocation = -1;
    m_textureLocation = -1;
    m_pointSizeLocation = -1;
}

void DrawProgram::Bind() const
{
    m_shader.Bind();
}

void DrawProgram::SetPositionOffset(float dx, float dy) const
{
    if (m_posOffsetLocation >= 0)
        glUniform2f(m_posOffsetLocation, dx, dy);
}

void DrawProgram::SetUvOffset(float du, float dv) const
{
    if (m_uvOffsetLocation >= 0)
        glUniform2f(m_uvOffsetLocation, du, dv);
}

void DrawProgram::SetPointSize(float size) const
{
    if (m_pointSizeLocation >= 0)
        glUniform1f(m_pointSizeLocation, size);
}

}
