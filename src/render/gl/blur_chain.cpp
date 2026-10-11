//
//  blur_chain.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "blur_chain.h"
#include "gl_error.h"
#include "gl_headers.h"
#include "shader_prologue.h"

#include <algorithm>
#include <cstdio>

namespace render_gl
{

namespace
{

const char* const kBlurVertexSource =
    "uniform vec2 uDestInv;\n"
    "out vec2 vUv;\n"
    "void main()\n"
    "{\n"
    "    vec2 pos[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));\n"
    "    vec2 uv[4] = vec2[4](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(1.0, 1.0));\n"
    "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"
    "    vUv = uv[gl_VertexID] - 0.5 * uDestInv;\n"
    "}\n";

const char* const kBlurHorizontalFragmentSource =
    "uniform sampler2D uSource;\n"
    "uniform vec4 _c0;\n"
    "uniform vec4 _c1;\n"
    "uniform vec4 _c2;\n"
    "uniform vec4 _c3;\n"
    "in vec2 vUv;\n"
    "out vec4 fragColor;\n"
    "vec3 tex2D(vec2 uv)\n"
    "{\n"
    "    return texture(uSource, vec2(uv.x, 1.0 - uv.y)).xyz;\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    vec2 uv2 = vUv + _c0.zw * vec2(1.0, 1.0);\n"
    "    vec3 blur =\n"
    "        (tex2D(uv2 + vec2( _c2.x * _c0.z, 0.0)) + tex2D(uv2 + vec2(-_c2.x * _c0.z, 0.0))) * _c1.x +\n"
    "        (tex2D(uv2 + vec2( _c2.y * _c0.z, 0.0)) + tex2D(uv2 + vec2(-_c2.y * _c0.z, 0.0))) * _c1.y +\n"
    "        (tex2D(uv2 + vec2( _c2.z * _c0.z, 0.0)) + tex2D(uv2 + vec2(-_c2.z * _c0.z, 0.0))) * _c1.z +\n"
    "        (tex2D(uv2 + vec2( _c2.w * _c0.z, 0.0)) + tex2D(uv2 + vec2(-_c2.w * _c0.z, 0.0))) * _c1.w;\n"
    "    blur *= _c3.z;\n"
    "    blur = blur * _c3.x + _c3.y;\n"
    "    fragColor = vec4(blur, 1.0);\n"
    "}\n";

const char* const kBlurVerticalFragmentSource =
    "uniform sampler2D uSource;\n"
    "uniform vec4 _c0;\n"
    "uniform vec4 _c5;\n"
    "uniform vec4 _c6;\n"
    "in vec2 vUv;\n"
    "out vec4 fragColor;\n"
    "vec3 tex2D(vec2 uv)\n"
    "{\n"
    "    return texture(uSource, vec2(uv.x, 1.0 - uv.y)).xyz;\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    vec2 uv2 = vUv + _c0.zw * vec2(1.0, 0.0);\n"
    "    vec3 blur =\n"
    "        (tex2D(uv2 + vec2(0.0,  _c5.z * _c0.w)) + tex2D(uv2 + vec2(0.0, -_c5.z * _c0.w))) * _c5.x +\n"
    "        (tex2D(uv2 + vec2(0.0,  _c5.w * _c0.w)) + tex2D(uv2 + vec2(0.0, -_c5.w * _c0.w))) * _c5.y;\n"
    "    blur *= _c6.x;\n"
    "    float t = min(min(vUv.x, vUv.y), 1.0 - max(vUv.x, vUv.y));\n"
    "    t = sqrt(max(t, 0.0));\n"
    "    t = _c6.y + _c6.z * clamp(t * _c6.w, 0.0, 1.0);\n"
    "    blur *= t;\n"
    "    fragColor = vec4(blur, 1.0);\n"
    "}\n";

const float kBlurWeights[8] = { 4.0f, 3.8f, 3.5f, 2.9f, 1.9f, 1.2f, 0.7f, 0.3f };

}

BlurChain::BlurChain()
{
    for (int i = 0; i < kBlurTextureCount; i++)
    {
        m_widths[i] = 0;
        m_heights[i] = 0;
    }

    LocateUniforms(m_passX, m_uniformsX);
    LocateUniforms(m_passY, m_uniformsY);
}

void BlurChain::LocateUniforms(const Shader& shader, PassUniforms& uniforms)
{
    if (!shader.Valid())
    {
        uniforms.c0 = uniforms.c1 = uniforms.c2 = uniforms.c3 = -1;
        uniforms.c5 = uniforms.c6 = uniforms.destInv = uniforms.source = -1;
        return;
    }

    const unsigned int program = shader.Id();
    uniforms.c0 = glGetUniformLocation(program, "_c0");
    uniforms.c1 = glGetUniformLocation(program, "_c1");
    uniforms.c2 = glGetUniformLocation(program, "_c2");
    uniforms.c3 = glGetUniformLocation(program, "_c3");
    uniforms.c5 = glGetUniformLocation(program, "_c5");
    uniforms.c6 = glGetUniformLocation(program, "_c6");
    uniforms.destInv = glGetUniformLocation(program, "uDestInv");
    uniforms.source = glGetUniformLocation(program, "uSource");
}

bool BlurChain::Create(int texSizeX, int texSizeY)
{
    Destroy();

    const ShaderBuildResult horizontal = m_passX.BuildFromSource(WithShaderPrologue(ShaderStage::Vertex, kBlurVertexSource).c_str(),
        WithShaderPrologue(ShaderStage::Fragment, kBlurHorizontalFragmentSource).c_str());
    if (!horizontal.ok)
    {
        fprintf(stderr, "render_gl: BlurChain horizontal pass build failed: %s\n", horizontal.log.c_str());
        Destroy();
        return false;
    }

    const ShaderBuildResult vertical = m_passY.BuildFromSource(WithShaderPrologue(ShaderStage::Vertex, kBlurVertexSource).c_str(),
        WithShaderPrologue(ShaderStage::Fragment, kBlurVerticalFragmentSource).c_str());
    if (!vertical.ok)
    {
        fprintf(stderr, "render_gl: BlurChain vertical pass build failed: %s\n", vertical.log.c_str());
        Destroy();
        return false;
    }

    LocateUniforms(m_passX, m_uniformsX);
    LocateUniforms(m_passY, m_uniformsY);

    if (!m_emptyArray.Create())
    {
        Destroy();
        return false;
    }

    int w = texSizeX;
    int h = texSizeY;
    for (int i = 0; i < kBlurTextureCount; i++)
    {
        if (!(i & 1) || (i < 2))
        {
            w = std::max(16, w / 2);
            h = std::max(16, h / 2);
        }

        const int w2 = ((w + 3) / 16) * 16;
        const int h2 = ((h + 3) / 4) * 4;

        if (!m_levels[i].Create(w2, h2))
        {
            fprintf(stderr, "render_gl: BlurChain level %d create failed at %dx%d\n", i, w2, h2);
            Destroy();
            return false;
        }

        m_widths[i] = w2;
        m_heights[i] = h2;
    }

    return CheckGlError("BlurChain::Create");
}

void BlurChain::Destroy()
{
    for (int i = 0; i < kBlurTextureCount; i++)
    {
        m_levels[i].Destroy();
        m_widths[i] = 0;
        m_heights[i] = 0;
    }

    m_emptyArray.Destroy();
    m_passX.Destroy();
    m_passY.Destroy();
}

bool BlurChain::Resize(int texSizeX, int texSizeY)
{
    return Create(texSizeX, texSizeY);
}

void BlurChain::Run(const BlurChainInput& input, const Texture& source)
{
    const int passes = std::min(kBlurTextureCount, input.highestBlurTexUsed * 2);
    if (passes <= 0 || !Valid())
        return;

    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    m_emptyArray.Bind();

    const float* w = kBlurWeights;
    const float* blur_min = input.blurMin;
    const float* blur_max = input.blurMax;

    float fscale[3];
    float fbias[3];

    float temp_min, temp_max;
    fscale[0] = 1.0f / (blur_max[0] - blur_min[0]);
    fbias [0] = -blur_min[0] * fscale[0];
    temp_min  = (blur_min[1] - blur_min[0]) / (blur_max[0] - blur_min[0]);
    temp_max  = (blur_max[1] - blur_min[0]) / (blur_max[0] - blur_min[0]);
    fscale[1] = 1.0f / (temp_max - temp_min);
    fbias [1] = -temp_min * fscale[1];
    temp_min  = (blur_min[2] - blur_min[1]) / (blur_max[1] - blur_min[1]);
    temp_max  = (blur_max[2] - blur_min[1]) / (blur_max[1] - blur_min[1]);
    fscale[2] = 1.0f / (temp_max - temp_min);
    fbias [2] = -temp_min * fscale[2];

    for (int i = 0; i < passes; i++)
    {
        const bool bHorizontal = (i % 2 == 0);
        const Shader& shader = bHorizontal ? m_passX : m_passY;
        const PassUniforms& uniforms = bHorizontal ? m_uniformsX : m_uniformsY;

        m_levels[i].Bind();
        shader.Bind();

        if (i == 0)
        {
            source.SetWrapRepeat(false);
            source.Bind();
        }
        else
            m_levels[i - 1].ColorTexture().Bind();

        const int srcw = (i == 0) ? input.sourceWidth : m_widths[i - 1];
        const int srch = (i == 0) ? input.sourceHeight : m_heights[i - 1];

        if (uniforms.source >= 0)
            glUniform1i(uniforms.source, 0);

        if (uniforms.destInv >= 0)
            glUniform2f(uniforms.destInv, 1.0f / static_cast<float>(m_widths[i]), 1.0f / static_cast<float>(m_heights[i]));

        if (uniforms.c0 >= 0)
            glUniform4f(uniforms.c0, static_cast<float>(srcw), static_cast<float>(srch),
                1.0f / static_cast<float>(srcw), 1.0f / static_cast<float>(srch));

        const float fscale_now = fscale[i / 2];
        const float fbias_now = fbias[i / 2];

        if (bHorizontal)
        {
            const float w1 = w[0] + w[1];
            const float w2 = w[2] + w[3];
            const float w3 = w[4] + w[5];
            const float w4 = w[6] + w[7];
            const float d1 = 0 + 2 * w[1] / w1;
            const float d2 = 2 + 2 * w[3] / w2;
            const float d3 = 4 + 2 * w[5] / w3;
            const float d4 = 6 + 2 * w[7] / w4;
            const float w_div = 0.5f / (w1 + w2 + w3 + w4);

            if (uniforms.c1 >= 0) glUniform4f(uniforms.c1, w1, w2, w3, w4);
            if (uniforms.c2 >= 0) glUniform4f(uniforms.c2, d1, d2, d3, d4);
            if (uniforms.c3 >= 0) glUniform4f(uniforms.c3, fscale_now, fbias_now, w_div, 0.0f);
        }
        else
        {
            const float w1 = w[0] + w[1] + w[2] + w[3];
            const float w2 = w[4] + w[5] + w[6] + w[7];
            const float d1 = 0 + 2 * ((w[2] + w[3]) / w1);
            const float d2 = 2 + 2 * ((w[6] + w[7]) / w2);
            const float w_div = 1.0f / ((w1 + w2) * 2);

            if (uniforms.c5 >= 0) glUniform4f(uniforms.c5, w1, w2, d1, d2);
            if (uniforms.c6 >= 0)
            {
                if (i == 1)
                    glUniform4f(uniforms.c6, w_div, 1 - input.edgeDarken, input.edgeDarken, 5.0f);
                else
                    glUniform4f(uniforms.c6, w_div, 1.0f, 0.0f, 5.0f);
            }
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

}
