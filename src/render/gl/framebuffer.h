//
//  framebuffer.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#ifndef __BEATDROP_RENDER_GL_FRAMEBUFFER_H__
#define __BEATDROP_RENDER_GL_FRAMEBUFFER_H__ 1

#include "texture.h"

namespace render_gl
{

class Framebuffer
{
public:
    Framebuffer();
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    bool Create(int width, int height);
    void Destroy();
    bool Resize(int width, int height);

    void Bind() const;
    void Unbind() const;

    Texture& ColorTexture() { return m_color; }
    const Texture& ColorTexture() const { return m_color; }

    unsigned int Id() const { return m_id; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    bool Valid() const { return m_id != 0; }

private:
    unsigned int m_id;
    int m_width;
    int m_height;
    Texture m_color;
};

}

#endif