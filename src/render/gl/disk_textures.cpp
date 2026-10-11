//
//  disk_textures.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "disk_textures.h"
#include "gl_error.h"
#include "gl_object_counts.h"
#include "gl_headers.h"
#include "rng_compat.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_MAX_DIMENSIONS 8192
#include "stb_image.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace render_gl
{

namespace
{

namespace fs = std::filesystem;

constexpr bool kFlipDiskTextureRows = false;

const char* const kTextureExtensions[] = {
    "jpg", "jpeg", "jfif", "dds", "png", "webp", "tga", "bmp", "dib", "gif",
    "webm", "mkv", "flv", "vob", "ogv", "mng", "avi", "mts", "m2ts", "ts",
    "mov", "qt", "wmv", "yuv", "rm", "rmvb", "viv", "asf", "amv", "mp4",
    "m4p", "mp2", "mpe", "mpv", "mpg", "mpeg", "m2v", "m4v", "svi", "3gp",
    "3g2", "mxf", "roq", "nsv", "f4v", "f4p", "f4a", "f4b"
};

bool EqualsIgnoreCase(const char* a, const char* b, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (toupper(static_cast<unsigned char>(a[i])) != toupper(static_cast<unsigned char>(b[i])))
            return false;
        if (a[i] == 0)
            return true;
    }

    return true;
}

bool IsTextureExtension(const std::string& szExt)
{
    for (const char* szKnown : kTextureExtensions)
    {
        if (szExt.size() == strlen(szKnown) && EqualsIgnoreCase(szKnown, szExt.c_str(), szExt.size()))
            return true;
    }

    return false;
}

bool UpperLess(const std::string& a, const std::string& b)
{
    const size_t common = std::min(a.size(), b.size());

    for (size_t i = 0; i < common; i++)
    {
        const int ca = toupper(static_cast<unsigned char>(a[i]));
        const int cb = toupper(static_cast<unsigned char>(b[i]));

        if (ca != cb)
            return ca < cb;
    }

    return a.size() < b.size();
}

constexpr std::uintmax_t kMaxTextureFileBytes = 64u * 1024u * 1024u;

bool ReadFileBytes(const fs::path& path, std::vector<unsigned char>& bytes)
{
    std::error_code sizeError;
    const std::uintmax_t fileSize = fs::file_size(path, sizeError);

    if (sizeError || fileSize > kMaxTextureFileBytes)
        return false;

    std::ifstream file(path, std::ios::in | std::ios::binary);

    if (!file)
        return false;

    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return !bytes.empty();
}

}

RandomTexturePicker::RandomTexturePicker()
    : m_gen(nullptr)
    , m_dirty(true)
    , m_sortedOrder(true)
{
}

void RandomTexturePicker::Configure(const std::string& texturesDir, std::mt19937_64* gen)
{
    m_texturesDir = texturesDir;
    m_gen = gen;
    m_dirty = true;
}

void RandomTexturePicker::ScanIfNeeded()
{
    if (!m_dirty)
        return;

    m_dirty = false;
    m_files.clear();

    std::error_code error;
    fs::directory_iterator it(fs::path(m_texturesDir), error);

    if (error)
        return;

    for (; it != fs::directory_iterator(); it.increment(error))
    {
        if (error)
            break;

        try
        {
            std::error_code entryError;

            if (!it->is_regular_file(entryError))
                continue;

            const std::string ext = it->path().extension().string();

            if (ext.size() < 2 || !IsTextureExtension(ext.substr(1)))
                continue;

            m_files.push_back(it->path().filename().string());
        }
        catch (const std::exception&)
        {
        }
    }

    if (m_sortedOrder)
        std::sort(m_files.begin(), m_files.end(), UpperLess);
}

size_t RandomTexturePicker::FileCount()
{
    ScanIfNeeded();
    return m_files.size();
}

bool RandomTexturePicker::PickRandomTexture(const char* szPrefix, char* szRetTextureFilename, int nMaxBytes)
{
    ScanIfNeeded();

    if (m_gen == nullptr || m_files.empty())
        return false;

    const std::string* picked = nullptr;

    if (szPrefix == nullptr || szPrefix[0] == 0)
    {
        BdUniformIntDistribution<size_t> dist(0, m_files.size() - 1);
        const int i = static_cast<int>(dist(*m_gen));
        picked = &m_files[i];
    }
    else
    {
        const size_t len = strlen(szPrefix);
        std::vector<const std::string*> subset;

        for (const std::string& file : m_files)
        {
            if (file.size() >= len && EqualsIgnoreCase(szPrefix, file.c_str(), len))
                subset.push_back(&file);
        }

        if (subset.empty())
            return false;

        BdUniformIntDistribution<size_t> dist(0, subset.size() - 1);
        const int i = static_cast<int>(dist(*m_gen));
        picked = subset[i];
    }

    if (picked->size() + 1 > static_cast<size_t>(nMaxBytes))
        return false;

    memcpy(szRetTextureFilename, picked->c_str(), picked->size() + 1);
    return true;
}

DiskTextureCache::DiskTextureCache()
    : m_generation(0)
{
}

DiskTextureCache::~DiskTextureCache()
{
    Destroy();
}

void DiskTextureCache::SetDirectories(const std::string& texturesDir, const std::string& presetDir, const std::string& searchDir)
{
    m_texturesDir = texturesDir;
    m_searchDir = searchDir;
    m_presetDir = presetDir;
}

bool DiskTextureCache::LoadFromFile(const std::string& path, DiskTexture& out)
{
    std::vector<unsigned char> bytes;

    if (!ReadFileBytes(fs::path(path), bytes))
        return false;

    int width = 0;
    int height = 0;
    int components = 0;

    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

    if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &components) ||
        width > maxTextureSize || height > maxTextureSize)
        return false;

    unsigned char* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &components, 4);

    if (pixels == nullptr)
        return false;

    if (kFlipDiskTextureRows)
    {
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        std::vector<unsigned char> row(rowBytes);

        for (int y = 0; y < height / 2; y++)
        {
            unsigned char* top = pixels + static_cast<size_t>(y) * rowBytes;
            unsigned char* bottom = pixels + static_cast<size_t>(height - 1 - y) * rowBytes;
            memcpy(row.data(), top, rowBytes);
            memcpy(top, bottom, rowBytes);
            memcpy(bottom, row.data(), rowBytes);
        }
    }

    unsigned int id = 0;
    glGenTextures(1, &id);
    NoteGlObjectCreated(GlObjectKind::Texture);

    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    if (!CheckGlError("DiskTextureCache::LoadFromFile"))
    {
        glDeleteTextures(1, &id);
        NoteGlObjectDeleted(GlObjectKind::Texture);
        return false;
    }

    out.id = id;
    out.width = width;
    out.height = height;
    out.path = path;
    return true;
}

const DiskTexture* DiskTextureCache::Acquire(const char* szRootName)
{
    const auto found = m_textures.find(szRootName);

    if (found != m_textures.end())
    {
        found->second.lastUsed = m_generation;
        return found->second.id != 0 ? &found->second : nullptr;
    }

    DiskTexture texture = { 0, 0, 0, std::string(), m_generation };

    for (const char* szExt : kTextureExtensions)
    {
        const std::string name = std::string(szRootName) + "." + szExt;
        const fs::path inTextures = fs::path(m_texturesDir) / name;
        std::error_code error;

        fs::path chosen;

        if (fs::exists(inTextures, error))
            chosen = inTextures;
        else if (!m_searchDir.empty() && fs::exists(fs::path(m_searchDir) / name, error))
            chosen = fs::path(m_searchDir) / name;
        else if (!m_presetDir.empty() && fs::exists(fs::path(m_presetDir) / name, error))
            chosen = fs::path(m_presetDir) / name;
        else
            continue;

        try
        {
            if (LoadFromFile(chosen.string(), texture))
                break;
        }
        catch (const std::exception&)
        {
        }
    }

    const auto inserted = m_textures.emplace(szRootName, texture);
    return inserted.first->second.id != 0 ? &inserted.first->second : nullptr;
}

void DiskTextureCache::TrimUnused()
{
    for (auto it = m_textures.begin(); it != m_textures.end();)
    {
        if (m_generation - it->second.lastUsed < 2)
        {
            ++it;
            continue;
        }

        if (it->second.id != 0)
        {
            glDeleteTextures(1, &it->second.id);
            NoteGlObjectDeleted(GlObjectKind::Texture);
        }

        it = m_textures.erase(it);
    }
}

void DiskTextureCache::Destroy()
{
    for (auto& entry : m_textures)
    {
        if (entry.second.id != 0)
        {
            glDeleteTextures(1, &entry.second.id);
            NoteGlObjectDeleted(GlObjectKind::Texture);
        }
    }

    m_textures.clear();
}

}
