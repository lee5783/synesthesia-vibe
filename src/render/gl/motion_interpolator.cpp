//
//  motion_interpolator.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-11-05.
//

#include "motion_interpolator.h"
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

constexpr int kMinFlowLevelSize = 8;
constexpr int kMotionTextureUnits = 4;
constexpr float kMinFraction = 1.0e-3f;

const char* const kMotionVertexSource =
    "void main()\n"
    "{\n"
    "    vec2 pos[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));\n"
    "    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);\n"
    "}\n";

const char* const kMotionBlendSource =
    "uniform sampler2D uPrevious;\n"
    "uniform sampler2D uNewest;\n"
    "uniform float uAlpha;\n"
    "out vec4 fragColor;\n"
    "void main()\n"
    "{\n"
    "    ivec2 texel = ivec2(gl_FragCoord.xy);\n"
    "    vec4 previous = texelFetch(uPrevious, texel, 0);\n"
    "    vec4 newest = texelFetch(uNewest, texel, 0);\n"
    "    fragColor = uAlpha <= 0.0 ? previous : mix(previous, newest, uAlpha);\n"
    "}\n";

const char* const kMotionCommonSource =
    "out vec4 fragColor;\n"
    "vec2 bdPack(float code)\n"
    "{\n"
    "    float c = clamp(floor(code + 0.5), 0.0, 65535.0);\n"
    "    float high = floor(c / 256.0);\n"
    "    return vec2(high, c - high * 256.0) / 255.0;\n"
    "}\n"
    "float bdUnpack(vec2 t)\n"
    "{\n"
    "    return floor(t.x * 255.0 + 0.5) * 256.0 + floor(t.y * 255.0 + 0.5);\n"
    "}\n"
    "vec4 bdEncodeField(vec2 v)\n"
    "{\n"
    "    return vec4(bdPack(v.x * 256.0 + 32768.0), bdPack(v.y * 256.0 + 32768.0));\n"
    "}\n"
    "vec2 bdDecodeField(vec4 t)\n"
    "{\n"
    "    return (vec2(bdUnpack(t.rg), bdUnpack(t.ba)) - 32768.0) / 256.0;\n"
    "}\n"
    "float bdLumaOf(vec3 c)\n"
    "{\n"
    "    return dot(c, vec3(0.2126, 0.7152, 0.0722));\n"
    "}\n"
    "vec2 bdFetchField(sampler2D field, ivec2 p, ivec2 size)\n"
    "{\n"
    "    return bdDecodeField(texelFetch(field, clamp(p, ivec2(0), size - 1), 0));\n"
    "}\n"
    "vec2 bdSampleField(sampler2D field, vec2 q, ivec2 size)\n"
    "{\n"
    "    vec2 g = q - 0.5;\n"
    "    vec2 b = floor(g);\n"
    "    vec2 w = g - b;\n"
    "    ivec2 i = ivec2(b);\n"
    "    vec2 f00 = bdFetchField(field, i, size);\n"
    "    vec2 f10 = bdFetchField(field, i + ivec2(1, 0), size);\n"
    "    vec2 f01 = bdFetchField(field, i + ivec2(0, 1), size);\n"
    "    vec2 f11 = bdFetchField(field, i + ivec2(1, 1), size);\n"
    "    return mix(mix(f00, f10, w.x), mix(f01, f11, w.x), w.y);\n"
    "}\n";

const char* const kMotionDownsampleSource =
    "uniform sampler2D uSource;\n"
    "uniform float uFromColor;\n"
    "void main()\n"
    "{\n"
    "    ivec2 p = ivec2(gl_FragCoord.xy) * 2;\n"
    "    float sum = 0.0;\n"
    "    for (int y = 0; y < 2; y++)\n"
    "    {\n"
    "        for (int x = 0; x < 2; x++)\n"
    "        {\n"
    "            vec4 t = texelFetch(uSource, p + ivec2(x, y), 0);\n"
    "            sum += uFromColor > 0.5 ? bdLumaOf(t.rgb) * 65535.0 : bdUnpack(t.rg);\n"
    "        }\n"
    "    }\n"
    "    fragColor = vec4(bdPack(sum * 0.25), 0.0, 1.0);\n"
    "}\n";

const char* const kMotionMatchSource =
    "uniform sampler2D uSource;\n"
    "uniform sampler2D uTarget;\n"
    "uniform sampler2D uCoarse;\n"
    "uniform ivec2 uSize;\n"
    "uniform ivec2 uCoarseSize;\n"
    "uniform float uHasCoarse;\n"
    "uniform float uRefine;\n"
    "const float kOffsetBias = 0.004;\n"
    "float bdLumaAt(sampler2D t, ivec2 p)\n"
    "{\n"
    "    return bdUnpack(texelFetch(t, clamp(p, ivec2(0), uSize - 1), 0).rg) / 65535.0;\n"
    "}\n"
    "float bdVertex(float l, float c, float r)\n"
    "{\n"
    "    float den = l - 2.0 * c + r;\n"
    "    return den > 0.000001 ? clamp(0.5 * (l - r) / den, -0.5, 0.5) : 0.0;\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    ivec2 p = ivec2(gl_FragCoord.xy);\n"
    "    vec2 predicted = uHasCoarse > 0.5 ? 2.0 * bdSampleField(uCoarse, gl_FragCoord.xy * 0.5, uCoarseSize) : vec2(0.0);\n"
    "    ivec2 base = ivec2(floor(predicted + 0.5));\n"
    "    float src[9];\n"
    "    float dst[49];\n"
    "    for (int y = 0; y < 3; y++)\n"
    "    {\n"
    "        for (int x = 0; x < 3; x++)\n"
    "            src[y * 3 + x] = bdLumaAt(uSource, p + ivec2(x - 1, y - 1));\n"
    "    }\n"
    "    for (int y = 0; y < 7; y++)\n"
    "    {\n"
    "        for (int x = 0; x < 7; x++)\n"
    "            dst[y * 7 + x] = bdLumaAt(uTarget, p + base + ivec2(x - 3, y - 3));\n"
    "    }\n"
    "    float sad[25];\n"
    "    int best = 12;\n"
    "    float bestCost = 1.0e30;\n"
    "    for (int dy = 0; dy < 5; dy++)\n"
    "    {\n"
    "        for (int dx = 0; dx < 5; dx++)\n"
    "        {\n"
    "            float s = 0.0;\n"
    "            for (int oy = 0; oy < 3; oy++)\n"
    "            {\n"
    "                for (int ox = 0; ox < 3; ox++)\n"
    "                    s += abs(src[oy * 3 + ox] - dst[(dy + oy) * 7 + dx + ox]);\n"
    "            }\n"
    "            sad[dy * 5 + dx] = s;\n"
    "            float cost = s + kOffsetBias * float(abs(dx - 2) + abs(dy - 2));\n"
    "            if (cost < bestCost)\n"
    "            {\n"
    "                bestCost = cost;\n"
    "                best = dy * 5 + dx;\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    int bx = best % 5;\n"
    "    int by = best / 5;\n"
    "    vec2 d = vec2(base + ivec2(bx - 2, by - 2));\n"
    "    if (uRefine > 0.5)\n"
    "    {\n"
    "        if (bx > 0 && bx < 4)\n"
    "            d.x += bdVertex(sad[best - 1], sad[best], sad[best + 1]);\n"
    "        if (by > 0 && by < 4)\n"
    "            d.y += bdVertex(sad[best - 5], sad[best], sad[best + 5]);\n"
    "    }\n"
    "    fragColor = bdEncodeField(d);\n"
    "}\n";

const char* const kMotionTrustSource =
    "uniform sampler2D uSource;\n"
    "uniform sampler2D uTarget;\n"
    "uniform sampler2D uForward;\n"
    "uniform sampler2D uBackward;\n"
    "uniform ivec2 uSize;\n"
    "uniform float uScale;\n"
    "const float kNoise = 0.03;\n"
    "const float kGainScale = 4.0;\n"
    "const float kTexture = 0.15;\n"
    "const float kStill = 0.2;\n"
    "const float kConsistency = 0.75;\n"
    "const float kFold = 0.5;\n"
    "const float kReachNear = 32.0;\n"
    "const float kReachFar = 48.0;\n"
    "float bdLumaAt(sampler2D t, ivec2 p)\n"
    "{\n"
    "    return bdUnpack(texelFetch(t, clamp(p, ivec2(0), uSize - 1), 0).rg) / 65535.0;\n"
    "}\n"
    "vec2 bdSymmetric(ivec2 p)\n"
    "{\n"
    "    return 0.5 * (bdFetchField(uForward, p, uSize) - bdFetchField(uBackward, p, uSize));\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    ivec2 p = ivec2(gl_FragCoord.xy);\n"
    "    vec2 forward = bdFetchField(uForward, p, uSize);\n"
    "    vec2 backward = bdFetchField(uBackward, p, uSize);\n"
    "    vec2 v = 0.5 * (forward - backward);\n"
    "    ivec2 vi = ivec2(floor(v + 0.5));\n"
    "    float centre = bdLumaAt(uSource, p);\n"
    "    float sad0 = 0.0;\n"
    "    float sadBest = 0.0;\n"
    "    float contrast = 0.0;\n"
    "    for (int y = -1; y <= 1; y++)\n"
    "    {\n"
    "        for (int x = -1; x <= 1; x++)\n"
    "        {\n"
    "            ivec2 o = p + ivec2(x, y);\n"
    "            float a = bdLumaAt(uSource, o);\n"
    "            sad0 += abs(a - bdLumaAt(uTarget, o));\n"
    "            sadBest += abs(a - bdLumaAt(uTarget, o + vi));\n"
    "            contrast += abs(a - centre);\n"
    "        }\n"
    "    }\n"
    "    if (contrast > kTexture && sad0 < kStill * contrast)\n"
    "    {\n"
    "        fragColor = bdEncodeField(vec2(0.0));\n"
    "        return;\n"
    "    }\n"
    "    float gain = clamp(kGainScale * (sad0 - sadBest) / (sad0 + sadBest + kNoise), 0.0, 1.0);\n"
    "    vec2 end = bdFetchField(uBackward, p + vi, uSize);\n"
    "    float error = length(forward + end) / kConsistency;\n"
    "    float consistency = 1.0 / (1.0 + error * error);\n"
    "    vec2 dx = 0.5 * (bdSymmetric(p + ivec2(1, 0)) - bdSymmetric(p - ivec2(1, 0)));\n"
    "    vec2 dy = 0.5 * (bdSymmetric(p + ivec2(0, 1)) - bdSymmetric(p - ivec2(0, 1)));\n"
    "    float slope = sqrt(dot(dx, dx) + dot(dy, dy)) / kFold;\n"
    "    float fold = 1.0 / (1.0 + slope * slope);\n"
    "    float reach = clamp((kReachFar - length(v) * uScale) / (kReachFar - kReachNear), 0.0, 1.0);\n"
    "    fragColor = bdEncodeField(v * (gain * consistency * fold * reach));\n"
    "}\n";

const char* const kMotionFieldSource =
    "uniform sampler2D uForward;\n"
    "uniform ivec2 uSize;\n"
    "uniform float uScale;\n"
    "void main()\n"
    "{\n"
    "    ivec2 p = ivec2(gl_FragCoord.xy);\n"
    "    vec3 tent = vec3(1.0, 2.0, 1.0);\n"
    "    vec2 sum = vec2(0.0);\n"
    "    for (int y = 0; y < 3; y++)\n"
    "    {\n"
    "        for (int x = 0; x < 3; x++)\n"
    "            sum += tent[x] * tent[y] * bdFetchField(uForward, p + ivec2(x - 1, y - 1), uSize);\n"
    "    }\n"
    "    vec2 v = sum / 16.0 * uScale;\n"
    "    float reach = max(abs(v.x), abs(v.y));\n"
    "    v *= reach > 31.75 ? 31.75 / reach : 1.0;\n"
    "    vec2 code = clamp(floor(v * 4.0 + 128.5), 0.0, 255.0);\n"
    "    fragColor = vec4(code / 255.0, 0.0, 1.0);\n"
    "}\n";

const char* const kMotionWarpSource =
    "uniform sampler2D uPrevious;\n"
    "uniform sampler2D uNewest;\n"
    "uniform sampler2D uField;\n"
    "uniform ivec2 uSize;\n"
    "uniform vec2 uDisplaySize;\n"
    "uniform float uScale;\n"
    "uniform float uFraction;\n"
    "void main()\n"
    "{\n"
    "    vec2 p = gl_FragCoord.xy;\n"
    "    vec2 v = (texture(uField, p / uScale / vec2(uSize)).rg * 255.0 - 128.0) * 0.25;\n"
    "    vec4 a = texture(uPrevious, (p - uFraction * v) / uDisplaySize);\n"
    "    vec4 b = texture(uNewest, (p + (1.0 - uFraction) * v) / uDisplaySize);\n"
    "    fragColor = mix(a, b, uFraction);\n"
    "}\n";

bool BuildMotionPass(Shader& shader, const char* szName, const char* szBody, bool bCommon)
{
    const std::string fragment = bCommon ? std::string(kMotionCommonSource) + szBody : std::string(szBody);
    const ShaderBuildResult result = shader.BuildFromSource(WithShaderPrologue(ShaderStage::Vertex, kMotionVertexSource).c_str(),
        WithShaderPrologue(ShaderStage::Fragment, fragment.c_str()).c_str());

    if (!result.ok)
        fprintf(stderr, "render_gl: MotionInterpolator %s pass build failed: %s\n", szName, result.log.c_str());

    return result.ok;
}

void BindTextureUnit(int unit, int location, const Texture& texture)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    texture.Bind();

    if (location >= 0)
        glUniform1i(location, unit);
}

void SetSizeUniform(int location, const Framebuffer& target)
{
    if (location >= 0)
        glUniform2i(location, target.Width(), target.Height());
}

void SetFloatUniform(int location, float value)
{
    if (location >= 0)
        glUniform1f(location, value);
}

void ReleaseTextureUnits()
{
    for (int unit = kMotionTextureUnits - 1; unit >= 0; unit--)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    glBindVertexArray(0);
}

}

MotionInterpolator::MotionInterpolator()
    : m_buildWidth(0)
    , m_buildHeight(0)
    , m_buildFlowPixels(0)
    , m_buildStage(-1)
    , m_lumaLevels(0)
    , m_flowFinest(0)
    , m_flowLevels(0)
    , m_flowValid(false)
    , m_newestIndex(0)
    , m_capturedCount(0)
    , m_fieldReady(false)
    , m_flowPlanned(false)
    , m_cutPending(false)
{
    m_pyramidBuilt[0] = false;
    m_pyramidBuilt[1] = false;
    m_slotCut[0] = false;
    m_slotCut[1] = false;
    m_slotTime[0] = 0.0f;
    m_slotTime[1] = 0.0f;
    LocateUniforms(m_blend, m_blendUniforms);
    LocateUniforms(m_downsample, m_downsampleUniforms);
    LocateUniforms(m_match, m_matchUniforms);
    LocateUniforms(m_trust, m_trustUniforms);
    LocateUniforms(m_field, m_fieldUniforms);
    LocateUniforms(m_warp, m_warpUniforms);
}

void MotionInterpolator::LocateUniforms(const Shader& shader, PassUniforms& uniforms)
{
    if (!shader.Valid())
    {
        uniforms.source = uniforms.target = uniforms.coarse = -1;
        uniforms.previous = uniforms.newest = uniforms.forward = uniforms.backward = uniforms.field = -1;
        uniforms.size = uniforms.coarseSize = uniforms.displaySize = -1;
        uniforms.hasCoarse = uniforms.refine = uniforms.fromColor = -1;
        uniforms.fraction = uniforms.scale = uniforms.alpha = -1;
        return;
    }

    const unsigned int program = shader.Id();
    uniforms.source = glGetUniformLocation(program, "uSource");
    uniforms.target = glGetUniformLocation(program, "uTarget");
    uniforms.coarse = glGetUniformLocation(program, "uCoarse");
    uniforms.previous = glGetUniformLocation(program, "uPrevious");
    uniforms.newest = glGetUniformLocation(program, "uNewest");
    uniforms.forward = glGetUniformLocation(program, "uForward");
    uniforms.backward = glGetUniformLocation(program, "uBackward");
    uniforms.field = glGetUniformLocation(program, "uField");
    uniforms.size = glGetUniformLocation(program, "uSize");
    uniforms.coarseSize = glGetUniformLocation(program, "uCoarseSize");
    uniforms.displaySize = glGetUniformLocation(program, "uDisplaySize");
    uniforms.hasCoarse = glGetUniformLocation(program, "uHasCoarse");
    uniforms.refine = glGetUniformLocation(program, "uRefine");
    uniforms.fromColor = glGetUniformLocation(program, "uFromColor");
    uniforms.fraction = glGetUniformLocation(program, "uFraction");
    uniforms.scale = glGetUniformLocation(program, "uScale");
    uniforms.alpha = glGetUniformLocation(program, "uAlpha");
}

void MotionInterpolator::BeginCreate(int width, int height, int flowPixels)
{
    Destroy();
    m_buildWidth = width;
    m_buildHeight = height;
    m_buildFlowPixels = flowPixels;
    m_buildStage = 0;
}

MotionInterpolator::BuildState MotionInterpolator::BuildNext()
{
    if (!Building())
        return Ready() ? BuildState::Ready : BuildState::Failed;

    const int stage = m_buildStage;

    if (!BuildStage(stage))
    {
        if (stage == 0)
        {
            Destroy();
            return BuildState::Failed;
        }

        DestroyFlow();
        m_buildStage = kBuildStageCount;
    }
    else
    {
        m_buildStage = stage == 0 && !m_flowPlanned ? kBuildStageCount : stage + 1;
    }

    if (m_buildStage == kBuildStageCount && !CheckGlError("MotionInterpolator::Create"))
    {
        Destroy();
        return BuildState::Failed;
    }

    return m_buildStage == kBuildStageCount ? BuildState::Ready : BuildState::Pending;
}

bool MotionInterpolator::BuildStage(int stage)
{
    switch (stage)
    {
    case 0:
        if (!BuildMotionPass(m_blend, "blend", kMotionBlendSource, false))
            return false;

        LocateUniforms(m_blend, m_blendUniforms);

        if (!m_emptyArray.Create() || !m_slots[0].Create(m_buildWidth, m_buildHeight) || !m_slots[1].Create(m_buildWidth, m_buildHeight))
        {
            fprintf(stderr, "render_gl: MotionInterpolator targets create failed at %dx%d\n", m_buildWidth, m_buildHeight);
            return false;
        }

        ResetHistory();
        m_flowPlanned = m_buildFlowPixels > 0 && PlanFlow();
        return true;
    case 1:
        if (!BuildMotionPass(m_downsample, "downsample", kMotionDownsampleSource, true))
            return false;
        LocateUniforms(m_downsample, m_downsampleUniforms);
        return true;
    case 2:
        if (!BuildMotionPass(m_match, "match", kMotionMatchSource, true))
            return false;
        LocateUniforms(m_match, m_matchUniforms);
        return true;
    case 3:
        if (!BuildMotionPass(m_trust, "trust", kMotionTrustSource, true))
            return false;
        LocateUniforms(m_trust, m_trustUniforms);
        return true;
    case 4:
        if (!BuildMotionPass(m_field, "field", kMotionFieldSource, true))
            return false;
        LocateUniforms(m_field, m_fieldUniforms);
        return true;
    case 5:
        if (!BuildMotionPass(m_warp, "warp", kMotionWarpSource, true))
            return false;
        LocateUniforms(m_warp, m_warpUniforms);
        return true;
    case 6:
        return CreateLumaSlot(0);
    case 7:
        return CreateLumaSlot(1);
    case 8:
        if (!CreateFieldTargets())
            return false;
        m_flowValid = true;
        return true;
    default:
        return false;
    }
}

bool MotionInterpolator::PlanFlow()
{
    int levels = 0;
    int finest = -1;
    int levelWidth = m_buildWidth / 2;
    int levelHeight = m_buildHeight / 2;

    while (levels < kMaxLumaLevels && levelWidth >= 1 && levelHeight >= 1)
    {
        if (finest >= 0 && (levels - finest >= 2 || std::min(levelWidth, levelHeight) < kMinFlowLevelSize))
            break;

        m_levelWidths[levels] = levelWidth;
        m_levelHeights[levels] = levelHeight;

        if (finest < 0 && static_cast<long long>(levelWidth) * levelHeight <= m_buildFlowPixels)
            finest = levels;

        levels++;
        levelWidth /= 2;
        levelHeight /= 2;
    }

    if (finest < 0)
    {
        fprintf(stderr, "render_gl: MotionInterpolator flow budget of %d pixels not reached at %dx%d\n", m_buildFlowPixels, m_buildWidth,
            m_buildHeight);
        return false;
    }

    m_lumaLevels = levels;
    m_flowFinest = finest;
    m_flowLevels = levels - finest;
    return true;
}

bool MotionInterpolator::CreateLumaSlot(int slot)
{
    for (int level = 0; level < m_lumaLevels; level++)
    {
        if (!m_luma[slot][level].Create(m_levelWidths[level], m_levelHeights[level]))
        {
            fprintf(stderr, "render_gl: MotionInterpolator luma level %d create failed at %dx%d\n", level, m_levelWidths[level],
                m_levelHeights[level]);
            return false;
        }
    }

    return true;
}

bool MotionInterpolator::CreateFieldTargets()
{
    for (int i = 0; i < m_flowLevels; i++)
    {
        const int level = m_flowFinest + i;

        if (!m_forward[i].Create(m_levelWidths[level], m_levelHeights[level]) || !m_backward[i].Create(m_levelWidths[level], m_levelHeights[level]))
        {
            fprintf(stderr, "render_gl: MotionInterpolator field level %d create failed at %dx%d\n", i, m_levelWidths[level],
                m_levelHeights[level]);
            return false;
        }
    }

    const int flowWidth = m_levelWidths[m_flowFinest];
    const int flowHeight = m_levelHeights[m_flowFinest];

    if (!m_smoothForward.Create(flowWidth, flowHeight) ||
        !m_warpField.Create(flowWidth, flowHeight))
    {
        fprintf(stderr, "render_gl: MotionInterpolator field targets create failed at %dx%d\n", flowWidth, flowHeight);
        return false;
    }

    return true;
}

bool MotionInterpolator::Resize(int width, int height, int flowPixels)
{
    if (!Ready())
        return false;

    const bool hadFlowPrograms = m_downsample.Valid() && m_match.Valid() && m_trust.Valid() && m_field.Valid() && m_warp.Valid();
    const bool wantsFlow = flowPixels > 0;

    DestroyFlowTargets();
    m_slots[0].Destroy();
    m_slots[1].Destroy();

    m_buildWidth = width;
    m_buildHeight = height;
    m_buildFlowPixels = flowPixels;

    if (!m_slots[0].Create(width, height) || !m_slots[1].Create(width, height))
    {
        fprintf(stderr, "render_gl: MotionInterpolator targets resize failed at %dx%d\n", width, height);
        return false;
    }

    ResetHistory();

    m_flowPlanned = wantsFlow && hadFlowPrograms && PlanFlow();

    if (m_flowPlanned && CreateLumaSlot(0) && CreateLumaSlot(1) && CreateFieldTargets())
        m_flowValid = true;
    else
        DestroyFlowTargets();

    return CheckGlError("MotionInterpolator::Resize");
}

void MotionInterpolator::DestroyFlowTargets()
{
    m_warpField.Destroy();
    m_smoothForward.Destroy();

    for (int i = 0; i < kMaxFlowLevels; i++)
    {
        m_backward[i].Destroy();
        m_forward[i].Destroy();
    }

    for (int slot = 0; slot < 2; slot++)
    {
        for (int level = 0; level < kMaxLumaLevels; level++)
            m_luma[slot][level].Destroy();
    }

    m_lumaLevels = 0;
    m_flowFinest = 0;
    m_flowLevels = 0;
    m_flowValid = false;
}

void MotionInterpolator::DestroyFlow()
{
    DestroyFlowTargets();
    m_warp.Destroy();
    m_field.Destroy();
    m_trust.Destroy();
    m_match.Destroy();
    m_downsample.Destroy();
}

void MotionInterpolator::Destroy()
{
    DestroyFlow();
    m_slots[0].Destroy();
    m_slots[1].Destroy();
    m_emptyArray.Destroy();
    m_blend.Destroy();
    m_buildStage = -1;
    m_flowPlanned = false;
    LocateUniforms(m_blend, m_blendUniforms);
    ResetHistory();
}

void MotionInterpolator::ResetHistory()
{
    m_newestIndex = 0;
    m_capturedCount = 0;
    m_fieldReady = false;
    m_cutPending = false;
    m_slotCut[0] = false;
    m_slotCut[1] = false;
    m_pyramidBuilt[0] = false;
    m_pyramidBuilt[1] = false;
    m_slotTime[0] = 0.0f;
    m_slotTime[1] = 0.0f;
}

void MotionInterpolator::CommitCapture(float simulationTime)
{
    m_newestIndex = 1 - m_newestIndex;
    m_slotTime[m_newestIndex] = simulationTime;
    m_pyramidBuilt[m_newestIndex] = false;
    m_slotCut[m_newestIndex] = m_capturedCount > 0 && m_cutPending;
    m_cutPending = false;
    m_fieldReady = false;

    if (m_capturedCount < 2)
        m_capturedCount++;
}

float MotionInterpolator::PresentedTime(float fraction) const
{
    if (!HasPrevious())
        return m_slotTime[m_newestIndex];

    const float previous = m_slotTime[1 - m_newestIndex];
    return previous + (m_slotTime[m_newestIndex] - previous) * fraction;
}

void MotionInterpolator::RunFlowStages()
{
    glDisable(GL_BLEND);
    m_emptyArray.Bind();

    const int previous = 1 - m_newestIndex;

    for (const int slot : {previous, m_newestIndex})
    {
        if (!m_pyramidBuilt[slot])
        {
            BuildPyramid(slot);
            m_pyramidBuilt[slot] = true;
        }
    }

    EstimateFlow();
    m_fieldReady = true;
    ReleaseTextureUnits();
}

void MotionInterpolator::BuildPyramid(int slot)
{
    m_downsample.Bind();

    for (int level = 0; level < m_lumaLevels; level++)
    {
        const Framebuffer& source = level == 0 ? m_slots[slot] : m_luma[slot][level - 1];

        m_luma[slot][level].Bind();
        SetFloatUniform(m_downsampleUniforms.fromColor, level == 0 ? 1.0f : 0.0f);
        BindTextureUnit(0, m_downsampleUniforms.source, source.ColorTexture());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

void MotionInterpolator::Match(int fromSlot, int toSlot, Framebuffer* fields)
{
    m_match.Bind();

    for (int i = m_flowLevels - 1; i >= 0; i--)
    {
        const int level = m_flowFinest + i;
        const Framebuffer& source = m_luma[fromSlot][level];
        const Framebuffer& target = m_luma[toSlot][level];
        const bool bHasCoarse = i + 1 < m_flowLevels;
        const Framebuffer& coarse = bHasCoarse ? fields[i + 1] : source;

        fields[i].Bind();
        SetSizeUniform(m_matchUniforms.size, source);
        SetSizeUniform(m_matchUniforms.coarseSize, coarse);
        SetFloatUniform(m_matchUniforms.hasCoarse, bHasCoarse ? 1.0f : 0.0f);
        SetFloatUniform(m_matchUniforms.refine, i == 0 ? 1.0f : 0.0f);
        BindTextureUnit(0, m_matchUniforms.source, source.ColorTexture());
        BindTextureUnit(1, m_matchUniforms.target, target.ColorTexture());
        BindTextureUnit(2, m_matchUniforms.coarse, coarse.ColorTexture());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

void MotionInterpolator::Trust(int fromSlot, int toSlot)
{
    const Framebuffer& source = m_luma[fromSlot][m_flowFinest];
    const Framebuffer& target = m_luma[toSlot][m_flowFinest];

    m_trust.Bind();
    m_smoothForward.Bind();
    SetSizeUniform(m_trustUniforms.size, m_smoothForward);
    SetFloatUniform(m_trustUniforms.scale, static_cast<float>(1 << (m_flowFinest + 1)));
    BindTextureUnit(0, m_trustUniforms.source, source.ColorTexture());
    BindTextureUnit(1, m_trustUniforms.target, target.ColorTexture());
    BindTextureUnit(2, m_trustUniforms.forward, m_forward[0].ColorTexture());
    BindTextureUnit(3, m_trustUniforms.backward, m_backward[0].ColorTexture());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void MotionInterpolator::EstimateFlow()
{
    const int previous = 1 - m_newestIndex;

    Match(previous, m_newestIndex, m_forward);
    Match(m_newestIndex, previous, m_backward);
    Trust(previous, m_newestIndex);

    m_field.Bind();
    m_warpField.Bind();
    SetSizeUniform(m_fieldUniforms.size, m_smoothForward);
    SetFloatUniform(m_fieldUniforms.scale, static_cast<float>(1 << (m_flowFinest + 1)));
    BindTextureUnit(0, m_fieldUniforms.forward, m_smoothForward.ColorTexture());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void MotionInterpolator::Present(float fraction, unsigned int targetFbo, bool useField)
{
    if (!Valid() || !HasHistory())
        return;

    const Framebuffer& newest = m_slots[m_newestIndex];

    if (!HasPrevious())
    {
        Blend(newest, newest, 0.0f, targetFbo);
        return;
    }

    const Framebuffer& previous = m_slots[1 - m_newestIndex];

    if (m_slotCut[m_newestIndex])
    {
        const Framebuffer& shown = fraction >= 1.0f ? newest : previous;
        Blend(shown, shown, 0.0f, targetFbo);
        return;
    }

    if (fraction < kMinFraction)
    {
        Blend(previous, previous, 0.0f, targetFbo);
        return;
    }

    if (!m_flowValid || !useField)
    {
        Blend(previous, newest, fraction, targetFbo);
        return;
    }

    if (!m_fieldReady)
        RunFlowStages();

    Warp(fraction, targetFbo);
}

void MotionInterpolator::Blend(const Framebuffer& previous, const Framebuffer& newest, float alpha, unsigned int targetFbo)
{
    glDisable(GL_BLEND);
    BindExternalTarget(targetFbo, newest.Width(), newest.Height());
    m_emptyArray.Bind();
    m_blend.Bind();

    SetFloatUniform(m_blendUniforms.alpha, alpha);
    BindTextureUnit(0, m_blendUniforms.previous, previous.ColorTexture());
    BindTextureUnit(1, m_blendUniforms.newest, newest.ColorTexture());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    ReleaseTextureUnits();
}

void MotionInterpolator::Warp(float fraction, unsigned int targetFbo)
{
    const Framebuffer& newest = m_slots[m_newestIndex];
    const Framebuffer& previous = m_slots[1 - m_newestIndex];

    glDisable(GL_BLEND);
    BindExternalTarget(targetFbo, newest.Width(), newest.Height());
    m_emptyArray.Bind();
    m_warp.Bind();

    SetSizeUniform(m_warpUniforms.size, m_smoothForward);

    if (m_warpUniforms.displaySize >= 0)
        glUniform2f(m_warpUniforms.displaySize, static_cast<float>(newest.Width()), static_cast<float>(newest.Height()));

    SetFloatUniform(m_warpUniforms.scale, static_cast<float>(1 << (m_flowFinest + 1)));
    SetFloatUniform(m_warpUniforms.fraction, fraction);
    BindTextureUnit(0, m_warpUniforms.previous, previous.ColorTexture());
    BindTextureUnit(1, m_warpUniforms.newest, newest.ColorTexture());
    BindTextureUnit(2, m_warpUniforms.field, m_warpField.ColorTexture());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    ReleaseTextureUnits();
}

}
