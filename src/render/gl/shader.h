//
//  shader.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __BEATDROP_RENDER_GL_SHADER_H__
#define __BEATDROP_RENDER_GL_SHADER_H__ 1

#include <string>

namespace render_gl
{

struct ShaderBuildResult
{
    bool ok;
    std::string log;
};

class Shader
{
public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    ShaderBuildResult BuildFromSource(const char* szVertexSource, const char* szFragmentSource);
    void Destroy();

    void Bind() const;

    unsigned int Id() const { return m_program; }
    bool Valid() const { return m_program != 0; }

private:
    unsigned int m_program;
};

}

#endif