//
//  luminance_limiter.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-03.
//

#ifndef __BEATDROP_RENDER_GL_LUMINANCE_LIMITER_H__
#define __BEATDROP_RENDER_GL_LUMINANCE_LIMITER_H__ 1

#include "framebuffer.h"
#include "shader.h"
#include "vertex_array.h"

namespace render_gl
{

struct LuminanceLimiterInput
{
    float maxLuminance;
    float flashLimit;
    float dtSeconds;
};

class LuminanceLimiter
{
public:
    LuminanceLimiter();

    bool Create(int width, int height);
    void Destroy();
    bool Resize(int width, int height);
    void ResetHistory();

    void Apply(const LuminanceLimiterInput& input, unsigned int targetFbo);

    unsigned int SceneFramebufferId() const { return m_scene.Id(); }
    bool Valid() const { return m_output.Valid() && m_scene.Valid(); }

private:
    struct PassUniforms
    {
        int scene;
        int mean;
        int history;
        int maxLuminance;
        int knee;
        int flashOn;
        int historyValid;
        int maxRise;
    };

    static void LocateUniforms(const Shader& shader, PassUniforms& uniforms);
    static void SetKneeUniforms(const PassUniforms& uniforms, float maxLuminance);

    Shader m_reduce;
    Shader m_history;
    Shader m_output;
    PassUniforms m_reduceUniforms;
    PassUniforms m_historyUniforms;
    PassUniforms m_outputUniforms;
    VertexArray m_emptyArray;
    Framebuffer m_scene;
    Framebuffer m_mean;
    Framebuffer m_smoothed[2];
    int m_smoothedIndex;
    bool m_historyValid;
};

}

#endif
