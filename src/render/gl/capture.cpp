//
//  capture.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "capture.h"
#include "gl_error.h"
#include "gl_headers.h"

#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace render_gl
{

bool ReadbackRgbTopDown(int width, int height, std::vector<unsigned char>& outPixels)
{
    if (width <= 0 || height <= 0)
        return false;

    const size_t pixelsPerRow = static_cast<size_t>(width);
    const size_t rowBytes = pixelsPerRow * 3;
    const size_t readRowBytes = pixelsPerRow * 4;
    const size_t totalBytes = rowBytes * static_cast<size_t>(height);

    std::vector<unsigned char> pixels(readRowBytes * static_cast<size_t>(height));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    if (!CheckGlError("ReadbackRgbTopDown glReadPixels"))
        return false;

    outPixels.assign(totalBytes, 0);

    for (int row = 0; row < height; ++row)
    {
        const unsigned char* src = pixels.data() + static_cast<size_t>(height - 1 - row) * readRowBytes;
        unsigned char* dst = outPixels.data() + static_cast<size_t>(row) * rowBytes;

        for (size_t x = 0; x < pixelsPerRow; ++x)
            memcpy(dst + x * 3, src + x * 4, 3);
    }

    return true;
}

bool WritePngRgbTopDown(const char* szPath, int width, int height, const std::vector<unsigned char>& pixelsTopDown)
{
    if (szPath == nullptr || width <= 0 || height <= 0)
        return false;

    const size_t rowBytes = static_cast<size_t>(width) * 3;

    if (pixelsTopDown.size() < rowBytes * static_cast<size_t>(height))
        return false;

    return stbi_write_png(szPath, width, height, 3, pixelsTopDown.data(), static_cast<int>(rowBytes)) != 0;
}

}