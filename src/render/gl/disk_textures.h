//
//  disk_textures.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_DISK_TEXTURES_H__
#define __BEATDROP_RENDER_GL_DISK_TEXTURES_H__ 1

#include <map>
#include <random>
#include <string>
#include <vector>

#include "sampler_resolver.h"

namespace render_gl
{

struct DiskTexture
{
    unsigned int id;
    int width;
    int height;
    std::string path;
    unsigned int lastUsed;
};

class RandomTexturePicker : public render_core::IRandomTextureSource
{
public:
    RandomTexturePicker();

    void Configure(const std::string& texturesDir, std::mt19937_64* gen);
    void SetSortedOrder(bool bSorted) { m_sortedOrder = bSorted; m_dirty = true; }
    void Rescan() { m_dirty = true; }
    bool PickRandomTexture(const char* szPrefix, char* szRetTextureFilename, int nMaxBytes) override;
    size_t FileCount();

private:
    void ScanIfNeeded();

    std::string m_texturesDir;
    std::mt19937_64* m_gen;
    std::vector<std::string> m_files;
    bool m_dirty;
    bool m_sortedOrder;
};

class DiskTextureCache
{
public:
    DiskTextureCache();
    ~DiskTextureCache();
    DiskTextureCache(const DiskTextureCache&) = delete;
    DiskTextureCache& operator=(const DiskTextureCache&) = delete;

    void SetDirectories(const std::string& texturesDir, const std::string& presetDir, const std::string& searchDir = std::string());
    const DiskTexture* Acquire(const char* szRootName);
    void BeginGeneration() { m_generation++; }
    void TrimUnused();
    size_t Size() const { return m_textures.size(); }
    void Destroy();

private:
    bool LoadFromFile(const std::string& path, DiskTexture& out);

    std::map<std::string, DiskTexture> m_textures;
    std::string m_texturesDir;
    std::string m_searchDir;
    std::string m_presetDir;
    unsigned int m_generation;
};

}

#endif
