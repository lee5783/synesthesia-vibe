//
//  shader_translator.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_SHADER_TRANSLATOR_H__
#define __BEATDROP_RENDER_GL_SHADER_TRANSLATOR_H__ 1

#include <cstddef>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

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

constexpr size_t kTranslatedShaderCacheCapacity = 64;

class TranslatedShaderCache
{
public:
    explicit TranslatedShaderCache(size_t capacity = kTranslatedShaderCacheCapacity);

    TranslatedShader GetOrTranslate(PresetShaderStage stage, const char* szPresetShaderText);

    size_t Size() const;
    int HitCount() const;
    int MissCount() const;

private:
    struct Entry
    {
        std::string glsl;
        std::list<const std::string*>::iterator order;
    };

    bool Find(const std::string& key, TranslatedShader& translated);
    void Insert(const std::string& key, const TranslatedShader& translated);

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Entry> m_entries;
    std::list<const std::string*> m_order;
    size_t m_capacity;
    int m_hits;
    int m_misses;
};

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
    const PresetProgram* GetOrBuildTranslated(PresetShaderStage stage, const char* szPresetShaderText,
        const TranslatedShader& translated);
    void SetTranslationCache(TranslatedShaderCache* translations) { m_translations = translations; }
    void Clear();
    void BeginGeneration() { m_generation++; }
    void TrimUnused();

    size_t Size() const { return m_entries.size(); }
    int HitCount() const { return m_hits; }
    int MissCount() const { return m_misses; }

private:
    const PresetProgram* Lookup(PresetShaderStage stage, const char* szPresetShaderText, const TranslatedShader* pTranslated);

    std::map<std::string, PresetProgram> m_entries;
    TranslatedShaderCache* m_translations;
    int m_hits;
    int m_misses;
    unsigned int m_generation;
};

}

#endif
