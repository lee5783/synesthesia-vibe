//
//  luminance_limiter.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-03.
//

#include "luminance_limiter.h"
#include "frame_targets.h"
#include "gl_error.h"
#include "gl_headers.h"
#include "shader_prologue.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace render_gl
{

namespace
{

constexpr float kLimiterKneeFraction = 0.75f;

const char* const kLimiterVertexSource =
    "void main()\n"
    "{\n"
    "    vec2 pos[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));\n"
    "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"
    "}\n";

const char* const kLimiterCommonSource =
    "uniform float uMaxLuminance;\n"
    "uniform float uKnee;\n"
    "out vec4 fragColor;\n"
    "float bdLuminance(vec3 c)\n"
    "{\n"
    "    return dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "}\n"
    "vec3 bdLimitPeak(vec3 c)\n"
    "{\n"
    "    float l = bdLuminance(c);\n"
    "    if (uMaxLuminance <= 0.0 || l <= uKnee)\n"
    "        return c;\n"
    "    float range = max(uMaxLuminance - uKnee, 0.00001);\n"
    "    float limited = uKnee + range * (1.0 - exp(-(l - uKnee) / range));\n"
    "    return c * (limited / l);\n"
    "}\n"
    "vec2 bdEncode(float v)\n"
    "{\n"
    "    float scaled = clamp(v, 0.0, 1.0) * 255.0;\n"
    "    float high = floor(scaled);\n"
    "    return vec2(high / 255.0, scaled - high);\n"
    "}\n"
    "float bdDecode(vec4 t)\n"
    "{\n"
    "    return (floor(t.r * 255.0 + 0.5) + t.g) / 255.0;\n"
    "}\n";

const char* const kLimiterReduceSource =
    "uniform sampler2D uScene;\n"
    "void main()\n"
    "{\n"
    "    float sum = 0.0;\n"
    "    for (int y = 0; y < 32; y++)\n"
    "    {\n"
    "        for (int x = 0; x < 32; x++)\n"
    "        {\n"
    "            vec2 uv = (vec2(float(x), float(y)) + 0.5) / 32.0;\n"
    "            sum += bdLuminance(bdLimitPeak(textureLod(uScene, uv, 0.0).rgb));\n"
    "        }\n"
    "    }\n"
    "    fragColor = vec4(bdEncode(sum / 1024.0), 0.0, 1.0);\n"
    "}\n";

const char* const kLimiterHistorySource =
    "uniform sampler2D uMean;\n"
    "uniform sampler2D uHistory;\n"
    "uniform float uHistoryValid;\n"
    "uniform float uMaxRise;\n"
    "void main()\n"
    "{\n"
    "    float mean = bdDecode(texelFetch(uMean, ivec2(0, 0), 0));\n"
    "    float previous = bdDecode(texelFetch(uHistory, ivec2(0, 0), 0));\n"
    "    float shown = uHistoryValid > 0.5 ? min(mean, previous + uMaxRise) : mean;\n"
    "    fragColor = vec4(bdEncode(shown), 0.0, 1.0);\n"
    "}\n";

const char* const kLimiterOutputSource =
    "uniform sampler2D uScene;\n"
    "uniform sampler2D uMean;\n"
    "uniform sampler2D uHistory;\n"
    "uniform float uFlashOn;\n"
    "void main()\n"
    "{\n"
    "    vec4 c = texelFetch(uScene, ivec2(gl_FragCoord.xy), 0);\n"
    "    vec3 rgb = bdLimitPeak(c.rgb);\n"
    "    if (uFlashOn > 0.5)\n"
    "    {\n"
    "        float mean = bdDecode(texelFetch(uMean, ivec2(0, 0), 0));\n"
    "        float shown = bdDecode(texelFetch(uHistory, ivec2(0, 0), 0));\n"
    "        rgb *= mean > 0.0001 ? min(shown / mean, 1.0) : 1.0;\n"
    "    }\n"
    "    fragColor = vec4(rgb, c.a);\n"
    "}\n";

bool BuildLimiterPass(Shader& shader, const char* szName, const char* szBody)
{
    const std::string fragment = std::string(kLimiterCommonSource) + szBody;
    const ShaderBuildResult result = shader.BuildFromSource(WithShaderPrologue(ShaderStage::Vertex, kLimiterVertexSource).c_str(),
        WithShaderPrologue(ShaderStage::Fragment, fragment.c_str()).c_str());

    if (!result.ok)
        fprintf(stderr, "render_gl: LuminanceLimiter %s pass build failed: %s\n", szName, result.log.c_str());

    return result.ok;
}

void BindTextureUnit(int unit, int location, const Texture& texture)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    texture.Bind();

    if (location >= 0)
        glUniform1i(location, unit);
}

}

LuminanceLimiter::LuminanceLimiter()
    : m_smoothedIndex(0)
    , m_historyValid(false)
{
    LocateUniforms(m_reduce, m_reduceUniforms);
    LocateUniforms(m_history, m_historyUniforms);
    LocateUniforms(m_output, m_outputUniforms);
}

void LuminanceLimiter::LocateUniforms(const Shader& shader, PassUniforms& uniforms)
{
    if (!shader.Valid())
    {
        uniforms.scene = uniforms.mean = uniforms.history = -1;
        uniforms.maxLuminance = uniforms.knee = uniforms.flashOn = -1;
        uniforms.historyValid = uniforms.maxRise = -1;
        return;
    }

    const unsigned int program = shader.Id();
    uniforms.scene = glGetUniformLocation(program, "uScene");
    uniforms.mean = glGetUniformLocation(program, "uMean");
    uniforms.history = glGetUniformLocation(program, "uHistory");
    uniforms.maxLuminance = glGetUniformLocation(program, "uMaxLuminance");
    uniforms.knee = glGetUniformLocation(program, "uKnee");
    uniforms.flashOn = glGetUniformLocation(program, "uFlashOn");
    uniforms.historyValid = glGetUniformLocation(program, "uHistoryValid");
    uniforms.maxRise = glGetUniformLocation(program, "uMaxRise");
}

void LuminanceLimiter::SetKneeUniforms(const PassUniforms& uniforms, float maxLuminance)
{
    if (uniforms.maxLuminance >= 0)
        glUniform1f(uniforms.maxLuminance, maxLuminance);

    if (uniforms.knee >= 0)
        glUniform1f(uniforms.knee, maxLuminance * kLimiterKneeFraction);
}

bool LuminanceLimiter::Create(int width, int height)
{
    Destroy();

    if (!BuildLimiterPass(m_reduce, "reduce", kLimiterReduceSource) ||
        !BuildLimiterPass(m_history, "history", kLimiterHistorySource) ||
        !BuildLimiterPass(m_output, "output", kLimiterOutputSource))
    {
        Destroy();
        return false;
    }

    LocateUniforms(m_reduce, m_reduceUniforms);
    LocateUniforms(m_history, m_historyUniforms);
    LocateUniforms(m_output, m_outputUniforms);

    if (!m_emptyArray.Create() || !m_scene.Create(width, height) || !m_mean.Create(1, 1) ||
        !m_smoothed[0].Create(1, 1) || !m_smoothed[1].Create(1, 1))
    {
        fprintf(stderr, "render_gl: LuminanceLimiter targets create failed at %dx%d\n", width, height);
        Destroy();
        return false;
    }

    ResetHistory();
    return CheckGlError("LuminanceLimiter::Create");
}

void LuminanceLimiter::Destroy()
{
    m_smoothed[0].Destroy();
    m_smoothed[1].Destroy();
    m_mean.Destroy();
    m_scene.Destroy();
    m_emptyArray.Destroy();
    m_output.Destroy();
    m_history.Destroy();
    m_reduce.Destroy();
    ResetHistory();
}

bool LuminanceLimiter::Resize(int width, int height)
{
    ResetHistory();

    if (!m_scene.Resize(width, height))
    {
        fprintf(stderr, "render_gl: LuminanceLimiter scene resize failed at %dx%d\n", width, height);
        return false;
    }

    return true;
}

void LuminanceLimiter::ResetHistory()
{
    m_smoothedIndex = 0;
    m_historyValid = false;
}

void LuminanceLimiter::Apply(const LuminanceLimiterInput& input, unsigned int targetFbo)
{
    if (!Valid())
        return;

    const bool bFlashOn = input.flashLimit > 0.0f;

    glDisable(GL_BLEND);
    m_emptyArray.Bind();

    if (bFlashOn)
    {
        m_mean.Bind();
        m_reduce.Bind();
        SetKneeUniforms(m_reduceUniforms, input.maxLuminance);
        BindTextureUnit(0, m_reduceUniforms.scene, m_scene.ColorTexture());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        const Framebuffer& previous = m_smoothed[m_smoothedIndex];
        Framebuffer& next = m_smoothed[1 - m_smoothedIndex];

        next.Bind();
        m_history.Bind();

        if (m_historyUniforms.historyValid >= 0)
            glUniform1f(m_historyUniforms.historyValid, m_historyValid ? 1.0f : 0.0f);

        if (m_historyUniforms.maxRise >= 0)
            glUniform1f(m_historyUniforms.maxRise, input.flashLimit * std::max(input.dtSeconds, 0.0f));

        BindTextureUnit(0, m_historyUniforms.mean, m_mean.ColorTexture());
        BindTextureUnit(1, m_historyUniforms.history, previous.ColorTexture());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        m_smoothedIndex = 1 - m_smoothedIndex;
        m_historyValid = true;
    }

    BindExternalTarget(targetFbo, m_scene.Width(), m_scene.Height());
    m_output.Bind();
    SetKneeUniforms(m_outputUniforms, input.maxLuminance);

    if (m_outputUniforms.flashOn >= 0)
        glUniform1f(m_outputUniforms.flashOn, bFlashOn ? 1.0f : 0.0f);

    BindTextureUnit(0, m_outputUniforms.scene, m_scene.ColorTexture());
    BindTextureUnit(1, m_outputUniforms.mean, m_mean.ColorTexture());
    BindTextureUnit(2, m_outputUniforms.history, m_smoothed[m_smoothedIndex].ColorTexture());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    for (int unit = 2; unit >= 0; unit--)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(0);
}

}
