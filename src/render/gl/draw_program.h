//
//  draw_program.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#ifndef __BEATDROP_RENDER_GL_DRAW_PROGRAM_H__
#define __BEATDROP_RENDER_GL_DRAW_PROGRAM_H__ 1

#include "shader.h"

namespace render_gl
{

class DrawProgram
{
public:
    DrawProgram();

    bool Build(bool bTextured);
    void Destroy();

    void Bind() const;

    void SetUvOffset(float du, float dv) const;
    void SetPositionOffset(float dx, float dy) const;
    void SetPointSize(float size) const;

    bool Textured() const { return m_bTextured; }
    bool Valid() const { return m_shader.Valid(); }

private:
    Shader m_shader;
    bool m_bTextured;
    int m_uvOffsetLocation;
    int m_posOffsetLocation;
    int m_textureLocation;
    int m_pointSizeLocation;
};

}

#endif
