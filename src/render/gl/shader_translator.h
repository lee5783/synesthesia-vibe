//
//  shader_translator.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_SHADER_TRANSLATOR_H__
#define __BEATDROP_RENDER_GL_SHADER_TRANSLATOR_H__ 1

#include <map>
#include <memory>
#include <string>

#include "shader.h"
#include "shader_prologue.h"

namespace render_gl
{

enum class PresetShaderStage
{
    Warp,
    Composite
};

struct TranslatedShader
{
    bool ok;
    std::string glsl;
    std::string error;
};

TranslatedShader TranslatePresetShader(PresetShaderStage stage, const char* szPresetShaderText);

TranslatedShader TranslatePresetShaderFor(GlslTarget target, PresetShaderStage stage, const char* szPresetShaderText);

const char* PresetVertexSource(PresetShaderStage stage);

void SetPresetPixelCenterOffset(int location, int width, int height);

struct PresetProgram
{
    PresetProgram();

    std::unique_ptr<Shader> shader;
    std::string glsl;
    std::string error;
    unsigned int lastUsed;
};

class PresetShaderCache
{
public:
    PresetShaderCache();

    const PresetProgram* GetOrBuild(PresetShaderStage stage, const char* szPresetShaderText);
    void Clear();
    void BeginGeneration() { m_generation++; }
    void TrimUnused();

    size_t Size() const { return m_entries.size(); }
    int HitCount() const { return m_hits; }
    int MissCount() const { return m_misses; }

private:
    std::map<std::string, PresetProgram> m_entries;
    int m_hits;
    int m_misses;
    unsigned int m_generation;
};

}

#endif
