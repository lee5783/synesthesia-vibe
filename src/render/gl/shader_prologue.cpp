//
//  shader_prologue.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-11.
//

#include "shader_prologue.h"

namespace render_gl
{

namespace
{

const char* const kDesktopVertexPrologue =
    "#version 330 core\n"
    "#define SV_COLOR_SWIZZLE(c) (c)\n";

const char* const kDesktopFragmentPrologue =
    "#version 330 core\n";

const char* const kEsVertexPrologue =
    "#version 300 es\n"
    "#define SV_COLOR_SWIZZLE(c) ((c).bgra)\n";

const char* const kEsFragmentPrologue =
    "#version 300 es\n"
    "precision highp float;\n"
    "precision highp int;\n"
    "precision highp sampler2D;\n"
    "precision highp sampler3D;\n";

}

GlslTarget DefaultGlslTarget()
{
#if defined(SYNESTHESIA_VIBE_GLES)
    return GlslTarget::Es300;
#else
    return GlslTarget::Desktop330;
#endif
}

const char* ShaderPrologue(ShaderStage stage, GlslTarget target)
{
    if (target == GlslTarget::Es300)
        return stage == ShaderStage::Vertex ? kEsVertexPrologue : kEsFragmentPrologue;

    return stage == ShaderStage::Vertex ? kDesktopVertexPrologue : kDesktopFragmentPrologue;
}

const char* ShaderPrologue(ShaderStage stage)
{
    return ShaderPrologue(stage, DefaultGlslTarget());
}

std::string WithShaderPrologue(ShaderStage stage, const char* szBody)
{
    return std::string(ShaderPrologue(stage)) + szBody;
}

}
