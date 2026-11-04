//
//  texture.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include "texture.h"
#include "gl_error.h"
#include "gl_object_counts.h"
#include "gl_headers.h"

namespace render_gl
{

namespace
{

void SpecifyStorage(TextureFormat format, int width, int height)
{
    if (format == TextureFormat::Rgba16F)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

}

bool HalfFloatColorTargetsSupported()
{
#if defined(SYNESTHESIA_VIBE_GLES)
    return GLAD_GL_EXT_color_buffer_half_float != 0 || GLAD_GL_EXT_color_buffer_float != 0;
#else
    return true;
#endif
}

const char* TextureFormatName(TextureFormat format)
{
    return (format == TextureFormat::Rgba16F) ? "RGBA16F" : "RGBA8";
}

Texture::Texture()
    : m_id(0)
    , m_width(0)
    , m_height(0)
    , m_format(TextureFormat::Rgba8)
{
}

Texture::~Texture()
{
    Destroy();
}

bool Texture::Create(int width, int height, TextureFormat format)
{
    Destroy();

    if (width <= 0 || height <= 0)
        return false;

    glGenTextures(1, &m_id);
    NoteGlObjectCreated(GlObjectKind::Texture);

    if (!CheckGlError("Texture::Create glGenTextures"))
        return false;

    Bind();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    SpecifyStorage(format, width, height);

    if (!CheckGlError("Texture::Create glTexImage2D"))
    {
        Destroy();
        return false;
    }

    m_width = width;
    m_height = height;
    m_format = format;
    return true;
}

void Texture::Destroy()
{
    if (m_id != 0)
    {
        glDeleteTextures(1, &m_id);
        NoteGlObjectDeleted(GlObjectKind::Texture);
        m_id = 0;
    }

    m_width = 0;
    m_height = 0;
    m_format = TextureFormat::Rgba8;
}

bool Texture::Resize(int width, int height)
{
    if (m_id != 0 && width == m_width && height == m_height)
        return true;

    return Create(width, height, m_format);
}

bool Texture::SetFormat(TextureFormat format)
{
    if (m_id == 0)
        return false;

    if (format == m_format)
        return true;

    Bind();
    SpecifyStorage(format, m_width, m_height);

    if (!CheckGlError("Texture::SetFormat glTexImage2D"))
        return false;

    m_format = format;
    return true;
}

void Texture::SetWrapRepeat(bool bRepeat) const
{
    Bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, bRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, bRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}

void Texture::Bind() const
{
    glBindTexture(GL_TEXTURE_2D, m_id);
}

}