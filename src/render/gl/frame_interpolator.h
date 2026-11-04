//
//  frame_interpolator.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-04.
//

#ifndef __BEATDROP_RENDER_GL_FRAME_INTERPOLATOR_H__
#define __BEATDROP_RENDER_GL_FRAME_INTERPOLATOR_H__ 1

#include "framebuffer.h"
#include "shader.h"
#include "vertex_array.h"

namespace render_gl
{

class FrameInterpolator
{
public:
    FrameInterpolator();

    bool Create(int width, int height);
    void Destroy();
    void ResetHistory();

    unsigned int CaptureFramebufferId() const { return m_slots[1 - m_newestIndex].Id(); }
    void CommitCapture();

    void Blend(float alpha, unsigned int targetFbo);

    bool HasHistory() const { return m_capturedCount > 0; }
    bool HasPrevious() const { return m_capturedCount > 1; }
    bool Valid() const { return m_blend.Valid() && m_slots[0].Valid() && m_slots[1].Valid(); }

private:
    Shader m_blend;
    VertexArray m_emptyArray;
    Framebuffer m_slots[2];
    int m_previousLocation;
    int m_newestLocation;
    int m_alphaLocation;
    int m_newestIndex;
    int m_capturedCount;
};

}

#endif
