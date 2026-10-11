//
//  shader_text.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-06.
//

#ifndef __BEATDROP_RENDER_CORE_SHADER_TEXT_H__
#define __BEATDROP_RENDER_CORE_SHADER_TEXT_H__ 1

namespace render_core
{

void StripComments(char* str);

bool ReadResourceToString(const char* szUtf8ResourceRoot, const char* szUtf8RelativeName, char* szDestText, int nMaxBytes, bool bConvertLFsToSpecialChar);

enum class ShaderStage
{
    Warp,
    Composite,
    Blur,
    Other,
    Unknown
};

const char* ShaderStageName(ShaderStage stage);

enum class ShaderTextResult
{
    Ok,
    ShaderBodyNotFound
};

struct ShaderTextInput
{
    const char* includeText;
    int includeTextLen;
    ShaderStage stage;
    bool bPixelProfile;
    const char* entryPoint;
    const char* shaderSource;
};

struct ShaderTextOutput
{
    int shaderStartPos;
    int textLength;
};

ShaderTextResult AssembleShaderText(const ShaderTextInput& input, char* szShaderText, ShaderTextOutput& out);

}

#endif