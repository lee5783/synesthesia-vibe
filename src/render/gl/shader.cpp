//
//  shader.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "shader.h"
#include "gl_error.h"
#include "gl_object_counts.h"
#include "gl_headers.h"

namespace render_gl
{

namespace
{

unsigned int CompileStage(unsigned int stage, const char* szSource, std::string& log)
{
    if (szSource == nullptr)
    {
        log += "render_gl: null shader source\n";
        return 0;
    }

    const unsigned int shader = glCreateShader(stage);
    glShaderSource(shader, 1, &szSource, nullptr);
    glCompileShader(shader);

    int status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == GL_TRUE)
        return shader;

    int logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 1)
    {
        std::string text(static_cast<size_t>(logLength), '\0');
        glGetShaderInfoLog(shader, logLength, nullptr, &text[0]);
        log += text.c_str();
    }
    else
    {
        log += "render_gl: shader compile failed with no info log\n";
    }

    glDeleteShader(shader);
    return 0;
}

}

Shader::Shader()
    : m_program(0)
{
}

Shader::~Shader()
{
    Destroy();
}

ShaderBuildResult Shader::BuildFromSource(const char* szVertexSource, const char* szFragmentSource)
{
    Destroy();

    ShaderBuildResult result;
    result.ok = false;

    const unsigned int vertex = CompileStage(GL_VERTEX_SHADER, szVertexSource, result.log);

    if (vertex == 0)
        return result;

    const unsigned int fragment = CompileStage(GL_FRAGMENT_SHADER, szFragmentSource, result.log);

    if (fragment == 0)
    {
        glDeleteShader(vertex);
        return result;
    }

    const unsigned int program = glCreateProgram();
    NoteGlObjectCreated(GlObjectKind::Program);
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    glDetachShader(program, vertex);
    glDetachShader(program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    int status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE)
    {
        int logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        if (logLength > 1)
        {
            std::string text(static_cast<size_t>(logLength), '\0');
            glGetProgramInfoLog(program, logLength, nullptr, &text[0]);
            result.log += text.c_str();
        }
        else
        {
            result.log += "render_gl: program link failed with no info log\n";
        }

        glDeleteProgram(program);
        NoteGlObjectDeleted(GlObjectKind::Program);
        return result;
    }

    if (!CheckGlError("Shader::BuildFromSource"))
    {
        glDeleteProgram(program);
        NoteGlObjectDeleted(GlObjectKind::Program);
        result.log += "render_gl: GL error during shader build\n";
        return result;
    }

    m_program = program;
    result.ok = true;
    return result;
}

void Shader::Destroy()
{
    if (m_program != 0)
    {
        glDeleteProgram(m_program);
        NoteGlObjectDeleted(GlObjectKind::Program);
        m_program = 0;
    }
}

void Shader::Bind() const
{
    glUseProgram(m_program);
}

}