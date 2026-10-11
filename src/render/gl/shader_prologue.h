//
//  shader_prologue.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#ifndef __BEATDROP_RENDER_GL_SHADER_PROLOGUE_H__
#define __BEATDROP_RENDER_GL_SHADER_PROLOGUE_H__ 1

#include <string>

namespace render_gl
{

enum class ShaderStage
{
    Vertex,
    Fragment
};

enum class GlslTarget
{
    Desktop330,
    Es300
};

GlslTarget DefaultGlslTarget();

const char* ShaderPrologue(ShaderStage stage, GlslTarget target);

const char* ShaderPrologue(ShaderStage stage);

std::string WithShaderPrologue(ShaderStage stage, const char* szBody);

}

#endif
