//
//  snapshot_writer.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-24.
//

#include "snapshot_writer.h"

#include "windows_headers.h"
#include "gl_headers.h"

#include "stb_image_write.h"

#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kRgbaChannels = 4;
    constexpr uint8_t kOpaqueAlpha = 255;

    void AppendBytes(void* context, void* data, int size)
    {
        auto* bytes = static_cast<std::vector<uint8_t>*>(context);
        const auto* begin = static_cast<const uint8_t*>(data);
        bytes->insert(bytes->end(), begin, begin + size);
    }

    std::wstring WidenUtf8(const char* text)
    {
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
        if (length <= 0)
            return std::wstring();

        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, &wide[0], length);
        wide.resize(static_cast<size_t>(length - 1));
        return wide;
    }
}

namespace svw
{

bool ReadFramebufferTopDown(uint32_t framebuffer, int width, int height, std::vector<uint8_t>& outRgba)
{
    if (width <= 0 || height <= 0)
        return false;

    GLint previousReadFramebuffer = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    const size_t rowBytes = static_cast<size_t>(width) * kRgbaChannels;
    std::vector<uint8_t> bottomUp(rowBytes * static_cast<size_t>(height));
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
    const bool ok = glGetError() == GL_NO_ERROR;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));

    if (!ok)
        return false;

    outRgba.resize(bottomUp.size());
    for (int row = 0; row < height; ++row)
    {
        const uint8_t* source = bottomUp.data() + static_cast<size_t>(height - 1 - row) * rowBytes;
        uint8_t* destination = outRgba.data() + static_cast<size_t>(row) * rowBytes;
        memcpy(destination, source, rowBytes);
        for (size_t alpha = kRgbaChannels - 1; alpha < rowBytes; alpha += kRgbaChannels)
            destination[alpha] = kOpaqueAlpha;
    }

    return true;
}

bool WritePngUtf8(const char* path, const std::vector<uint8_t>& rgba, int width, int height, std::string& error)
{
    const std::wstring widePath = WidenUtf8(path);
    if (widePath.empty())
    {
        error = "snapshot path is empty or not valid UTF-8";
        return false;
    }

    std::vector<uint8_t> encoded;
    if (stbi_write_png_to_func(AppendBytes, &encoded, width, height, kRgbaChannels, rgba.data(), width * kRgbaChannels) == 0)
    {
        error = "PNG encoding failed";
        return false;
    }

    FILE* file = nullptr;
    if (_wfopen_s(&file, widePath.c_str(), L"wb") != 0 || file == nullptr)
    {
        error = std::string("could not open ") + path + " for writing";
        return false;
    }

    const bool written = fwrite(encoded.data(), 1, encoded.size(), file) == encoded.size();
    const bool closed = fclose(file) == 0;
    if (!written || !closed)
    {
        error = std::string("could not write ") + path;
        return false;
    }

    return true;
}

}
