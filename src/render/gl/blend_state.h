//
//  blend_state.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-08.
//

#ifndef __BEATDROP_RENDER_GL_BLEND_STATE_H__
#define __BEATDROP_RENDER_GL_BLEND_STATE_H__ 1

namespace render_gl
{

enum class BlendFactor
{
    Zero,
    One,
    SrcAlpha,
    InvSrcAlpha,
    DstAlpha,
    InvDstAlpha,
    SrcColor,
    InvSrcColor,
    DstColor,
    InvDstColor
};

void SetBlendState(bool bEnable, BlendFactor src, BlendFactor dst);

void SetBlendOpaque();

void SetBlendAlpha();

void SetBlendAlphaFlipped();

void SetBlendAdditive();

void SetDefaultDrawState();

void SetPointSize(float size);

}

#endif
