//
//  texture.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __BEATDROP_RENDER_GL_TEXTURE_H__
#define __BEATDROP_RENDER_GL_TEXTURE_H__ 1

namespace render_gl
{

enum class TextureFormat
{
    Rgba8,
    Rgba16F
};

bool HalfFloatColorTargetsSupported();

const char* TextureFormatName(TextureFormat format);

class Texture
{
public:
    Texture();
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool Create(int width, int height, TextureFormat format = TextureFormat::Rgba8);
    void Destroy();
    bool Resize(int width, int height);
    bool SetFormat(TextureFormat format);

    void Bind() const;
    void SetWrapRepeat(bool bRepeat) const;

    unsigned int Id() const { return m_id; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    TextureFormat Format() const { return m_format; }
    bool Valid() const { return m_id != 0; }

private:
    unsigned int m_id;
    int m_width;
    int m_height;
    TextureFormat m_format;
};

}

#endif