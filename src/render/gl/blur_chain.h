//
//  blur_chain.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#ifndef __BEATDROP_RENDER_GL_BLUR_CHAIN_H__
#define __BEATDROP_RENDER_GL_BLUR_CHAIN_H__ 1

#include "framebuffer.h"
#include "shader.h"
#include "vertex_array.h"

namespace render_gl
{

constexpr int kBlurTextureCount = 6;

struct BlurChainInput
{
    float edgeDarken;
    float blurMin[3];
    float blurMax[3];
    int highestBlurTexUsed;
    int sourceWidth;
    int sourceHeight;
};

class BlurChain
{
public:
    BlurChain();

    bool Create(int texSizeX, int texSizeY);
    void Destroy();
    bool Resize(int texSizeX, int texSizeY);

    void Run(const BlurChainInput& input, const Texture& source);

    const Framebuffer& Level(int index) const { return m_levels[index]; }
    int Width(int index) const { return m_widths[index]; }
    int Height(int index) const { return m_heights[index]; }
    bool Valid() const { return m_passX.Valid() && m_passY.Valid() && m_levels[kBlurTextureCount - 1].Valid(); }

private:
    struct PassUniforms
    {
        int c0;
        int c1;
        int c2;
        int c3;
        int c5;
        int c6;
        int destInv;
        int source;
    };

    static void LocateUniforms(const Shader& shader, PassUniforms& uniforms);

    Shader m_passX;
    Shader m_passY;
    PassUniforms m_uniformsX;
    PassUniforms m_uniformsY;
    VertexArray m_emptyArray;
    Framebuffer m_levels[kBlurTextureCount];
    int m_widths[kBlurTextureCount];
    int m_heights[kBlurTextureCount];
};

}

#endif
