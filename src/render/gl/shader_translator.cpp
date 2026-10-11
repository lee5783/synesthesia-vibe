//
//  shader_translator.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#include "shader_translator.h"
#include "gl_headers.h"
#include "preset_shader_header.h"
#include "shader_prologue.h"
#include "shader_text.h"

#include "GLSLGenerator.h"
#include "HLSLParser.h"
#include "HLSLTree.h"

#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>

namespace M4
{

extern thread_local std::string g_lastLogError;

}

namespace render_gl
{

namespace
{

constexpr int kAssembledTextCapacity = 192000;
constexpr const char* kEntryPoint = "PS";
constexpr float kPixelCenterNdc = 0.9f;

const char* StageName(PresetShaderStage stage)
{
    return stage == PresetShaderStage::Warp ? "warp" : "comp";
}

render_core::ShaderStage CoreStage(PresetShaderStage stage)
{
    return stage == PresetShaderStage::Warp ? render_core::ShaderStage::Warp : render_core::ShaderStage::Composite;
}

std::string LastParserError(const char* szFallback)
{
    return M4::g_lastLogError.empty() ? std::string(szFallback) : M4::g_lastLogError;
}

TranslatedShader Failure(PresetShaderStage stage, const std::string& detail)
{
    TranslatedShader result;
    result.ok = false;
    result.error = std::string(StageName(stage)) + " shader translate failed: " + detail;
    return result;
}

bool StartsWithGlobalType(const std::string& line)
{
    static const char* const kTypePrefixes[] = {
        "sampler", "vec", "ivec", "uvec", "bvec", "mat", "float ", "int ", "uint ", "bool "
    };

    for (const char* prefix : kTypePrefixes)
    {
        if (line.compare(0, strlen(prefix), prefix) == 0)
            return true;
    }

    return false;
}

bool IsIdentifierChar(char c)
{
    return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::vector<std::string> DeclaredNames(const std::string& line)
{
    std::vector<std::string> names;
    const size_t end = line.find(';');
    size_t segmentStart = 0;

    while (segmentStart < end)
    {
        size_t segmentEnd = line.find(',', segmentStart);

        if (segmentEnd == std::string::npos || segmentEnd > end)
            segmentEnd = end;

        std::string segment = line.substr(segmentStart, segmentEnd - segmentStart);
        const size_t bracket = segment.find('[');

        if (bracket != std::string::npos)
            segment.resize(bracket);

        while (!segment.empty() && isspace(static_cast<unsigned char>(segment.back())))
            segment.pop_back();

        size_t nameStart = segment.size();

        while (nameStart > 0 && IsIdentifierChar(segment[nameStart - 1]))
            nameStart--;

        if (nameStart < segment.size())
            names.push_back(segment.substr(nameStart));

        segmentStart = segmentEnd + 1;
    }

    return names;
}

bool IsTypeWord(const std::string& word)
{
    static const char* const kPrefixes[] = { "vec", "ivec", "uvec", "bvec", "mat" };

    if (word == "float" || word == "int" || word == "uint" || word == "bool")
        return true;

    for (const char* prefix : kPrefixes)
    {
        if (word.compare(0, strlen(prefix), prefix) == 0)
            return true;
    }

    return false;
}

bool IsDeclarationOccurrence(const std::string& text, size_t position)
{
    size_t cursor = position;

    while (cursor > 0 && text[cursor - 1] == ' ')
        cursor--;

    size_t wordStart = cursor;

    while (wordStart > 0 && IsIdentifierChar(text[wordStart - 1]))
        wordStart--;

    return wordStart < cursor && IsTypeWord(text.substr(wordStart, cursor - wordStart));
}

bool IsAssignedInText(const std::string& text, const std::string& name)
{
    size_t position = 0;

    while ((position = text.find(name, position)) != std::string::npos)
    {
        const size_t after = position + name.size();
        const bool bBoundary = (position == 0 || !IsIdentifierChar(text[position - 1])) &&
            (after >= text.size() || !IsIdentifierChar(text[after]));

        position = after;

        if (!bBoundary || IsDeclarationOccurrence(text, after - name.size()))
            continue;

        size_t cursor = after;

        while (cursor < text.size())
        {
            if (text[cursor] == '.')
            {
                cursor++;

                while (cursor < text.size() && IsIdentifierChar(text[cursor]))
                    cursor++;
            }
            else if (text[cursor] == '[')
            {
                const size_t close = text.find(']', cursor);

                if (close == std::string::npos)
                    break;

                cursor = close + 1;
            }
            else
            {
                break;
            }
        }

        while (cursor < text.size() && text[cursor] == ' ')
            cursor++;

        if (cursor >= text.size())
            continue;

        if (text[cursor] == '=' && (cursor + 1 >= text.size() || text[cursor + 1] != '='))
            return true;

        if (cursor + 1 < text.size() && text[cursor + 1] == '=' && strchr("+-*/", text[cursor]) != nullptr)
            return true;
    }

    return false;
}

std::string PrefixGlobalUniforms(const std::string& glsl)
{
    std::istringstream input(glsl);
    std::string output;
    std::string line;
    int depth = 0;

    while (std::getline(input, line))
    {
        bool bGlobalDeclaration = depth == 0 && StartsWithGlobalType(line) &&
            line.find('(') == std::string::npos && line.find('=') == std::string::npos && line.find(';') != std::string::npos;

        if (bGlobalDeclaration && line.compare(0, 7, "sampler") != 0)
        {
            for (const std::string& name : DeclaredNames(line))
            {
                if (IsAssignedInText(glsl, name))
                {
                    bGlobalDeclaration = false;
                    break;
                }
            }
        }

        if (bGlobalDeclaration)
            output += "uniform ";

        output += line;
        output += "\n";

        for (char c : line)
        {
            if (c == '{')
                ++depth;
            else if (c == '}')
                --depth;
        }
    }

    return output;
}

bool IsFrameBufferSampler(const std::string& name)
{
    static const char* const kNames[] = {
        "sampler_main", "sampler_fc_main", "sampler_pc_main", "sampler_fw_main", "sampler_pw_main",
        "sampler_blur1", "sampler_blur2", "sampler_blur3", "sampler_blur4", "sampler_blur5", "sampler_blur6"
    };

    for (const char* known : kNames)
    {
        if (name == known)
            return true;
    }

    return false;
}

std::string FlipFrameBufferLookups(const std::string& glsl)
{
    static const std::string kCall = "texture(";
    static const char* const kFlipHelper = "vec2 bdFlipV(vec2 c) { return vec2(c.x, 1.0 - c.y); }\n";

    std::string output;
    size_t cursor = 0;

    while (true)
    {
        const size_t call = glsl.find(kCall, cursor);

        if (call == std::string::npos)
            break;

        const size_t nameStart = call + kCall.size();
        size_t nameEnd = nameStart;

        while (nameEnd < glsl.size() && (isalnum(static_cast<unsigned char>(glsl[nameEnd])) || glsl[nameEnd] == '_'))
            nameEnd++;

        const size_t comma = glsl.find(',', nameEnd);

        if (comma == std::string::npos || glsl.find_first_not_of(" ", nameEnd) != comma ||
            !IsFrameBufferSampler(glsl.substr(nameStart, nameEnd - nameStart)))
        {
            output.append(glsl, cursor, nameStart - cursor);
            cursor = nameStart;
            continue;
        }

        int depth = 1;
        size_t close = comma + 1;

        while (close < glsl.size() && depth > 0)
        {
            if (glsl[close] == '(')
                depth++;
            else if (glsl[close] == ')')
                depth--;

            if (depth > 0)
                close++;
        }

        if (depth != 0)
            break;

        output.append(glsl, cursor, comma + 1 - cursor);
        output += " bdFlipV(";
        output.append(glsl, comma + 1, close - comma - 1);
        output += ")";
        cursor = close;
    }

    output.append(glsl, cursor, std::string::npos);

    static const std::string kPrecision = "precision ";

    size_t insertAt = output.find('\n');

    while (insertAt != std::string::npos && output.compare(insertAt + 1, kPrecision.size(), kPrecision) == 0)
        insertAt = output.find('\n', insertAt + 1);

    if (insertAt != std::string::npos)
        output.insert(insertAt + 1, kFlipHelper);

    return output;
}

const char* const kWarpVertexSource =
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "layout(location = 2) in vec2 aUv;\n"
    "layout(location = 3) in vec2 aUvOrig;\n"
    "layout(location = 4) in vec2 aRadAng;\n"
    "uniform vec2 uPosOffset;\n"
    "out vec4 frag_COLOR;\n"
    "out vec4 frag_TEXCOORD0;\n"
    "out vec2 frag_TEXCOORD1;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPosition.x + uPosOffset.x, aPosition.y + uPosOffset.y, aPosition.z, 1.0);\n"
    "    frag_COLOR = SV_COLOR_SWIZZLE(aColor);\n"
    "    frag_TEXCOORD0 = vec4(aUv.x, aUv.y, aUvOrig.x, aUvOrig.y);\n"
    "    frag_TEXCOORD1 = aRadAng;\n"
    "}\n";

const char* const kCompVertexSource =
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "layout(location = 2) in vec2 aUv;\n"
    "layout(location = 4) in vec2 aRadAng;\n"
    "uniform vec2 uPosOffset;\n"
    "out vec4 frag_COLOR;\n"
    "out vec2 frag_TEXCOORD0;\n"
    "out vec2 frag_TEXCOORD1;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPosition.x + uPosOffset.x, aPosition.y + uPosOffset.y, aPosition.z, 1.0);\n"
    "    frag_COLOR = SV_COLOR_SWIZZLE(aColor);\n"
    "    frag_TEXCOORD0 = vec2(aUv.x, aUv.y);\n"
    "    frag_TEXCOORD1 = aRadAng;\n"
    "}\n";

std::string CompleteEsPrecisionBlock(const std::string& glsl)
{
    static const std::string kPrecision = "precision ";
    static const char* const kRequired[] = {
        "precision highp float;",
        "precision highp int;",
        "precision highp sampler2D;",
        "precision highp sampler3D;",
    };

    size_t blockEnd = glsl.find('\n');

    if (blockEnd == std::string::npos)
        return glsl;

    const size_t blockStart = blockEnd + 1;

    while (blockEnd != std::string::npos && glsl.compare(blockEnd + 1, kPrecision.size(), kPrecision) == 0)
        blockEnd = glsl.find('\n', blockEnd + 1);

    if (blockEnd == std::string::npos)
        return glsl;

    const std::string block = glsl.substr(blockStart, blockEnd + 1 - blockStart);
    std::string missing;

    for (const char* szLine : kRequired)
    {
        if (block.find(std::string(szLine) + "\n") == std::string::npos)
            missing += std::string(szLine) + "\n";
    }

    std::string output(glsl);
    output.insert(blockEnd + 1, missing);
    return output;
}

M4::GLSLGenerator::Version GeneratorVersion(GlslTarget target)
{
    return target == GlslTarget::Es300 ? M4::GLSLGenerator::Version_300_ES : M4::GLSLGenerator::Version_330;
}

}

TranslatedShader TranslatePresetShaderFor(GlslTarget target, PresetShaderStage stage, const char* szPresetShaderText)
{
    if (szPresetShaderText == nullptr || szPresetShaderText[0] == '\0')
        return Failure(stage, "empty shader text");

    const char* szHeader = PresetShaderHeaderText();

    std::vector<char> assembled(kAssembledTextCapacity, '\0');

    render_core::ShaderTextInput input;
    input.includeText = szHeader;
    input.includeTextLen = static_cast<int>(strlen(szHeader));
    input.stage = CoreStage(stage);
    input.bPixelProfile = true;
    input.entryPoint = kEntryPoint;
    input.shaderSource = szPresetShaderText;

    if (input.includeTextLen + static_cast<int>(strlen(szPresetShaderText)) * 2 + 1024 > kAssembledTextCapacity)
        return Failure(stage, "shader text too long");

    render_core::ShaderTextOutput output;

    if (render_core::AssembleShaderText(input, assembled.data(), output) != render_core::ShaderTextResult::Ok)
        return Failure(stage, "shader_body marker not found");

    const std::string shaderId = std::string(StageName(stage)) + "_ps";
    const size_t textLength = static_cast<size_t>(output.textLength);

    M4::g_lastLogError.clear();

    M4::Allocator allocator;
    M4::HLSLTree tree(&allocator);
    M4::HLSLParser parser(&allocator, &tree);

    std::string preprocessed;

    if (!parser.ApplyPreprocessor(shaderId.c_str(), assembled.data(), textLength, preprocessed))
        return Failure(stage, LastParserError("preprocessing failed"));

    if (!parser.Parse(shaderId.c_str(), preprocessed.c_str(), preprocessed.size()))
        return Failure(stage, LastParserError("HLSL parse failed"));

    M4::g_lastLogError.clear();

    M4::GLSLGenerator generator;

    if (!generator.Generate(&tree, M4::GLSLGenerator::Target_FragmentShader, GeneratorVersion(target), kEntryPoint, M4::GLSLGenerator::Options(M4::GLSLGenerator::Flag_AlternateNanPropagation)))
        return Failure(stage, LastParserError("GLSL generation failed"));

    TranslatedShader result;
    result.ok = true;
    const std::string generated = PrefixGlobalUniforms(generator.GetResult());
    result.glsl = FlipFrameBufferLookups(target == GlslTarget::Es300 ? CompleteEsPrecisionBlock(generated) : generated);
    return result;
}

TranslatedShader TranslatePresetShader(PresetShaderStage stage, const char* szPresetShaderText)
{
    return TranslatePresetShaderFor(DefaultGlslTarget(), stage, szPresetShaderText);
}

const char* PresetVertexSource(PresetShaderStage stage)
{
    static const std::string warpSource = WithShaderPrologue(ShaderStage::Vertex, kWarpVertexSource);
    static const std::string compSource = WithShaderPrologue(ShaderStage::Vertex, kCompVertexSource);

    return stage == PresetShaderStage::Warp ? warpSource.c_str() : compSource.c_str();
}

void SetPresetPixelCenterOffset(int location, int width, int height)
{
    if (location >= 0)
        glUniform2f(location, kPixelCenterNdc / static_cast<float>(width), -kPixelCenterNdc / static_cast<float>(height));
}

PresetProgram::PresetProgram()
    : lastUsed(0)
{
}

PresetShaderCache::PresetShaderCache()
    : m_hits(0)
    , m_misses(0)
    , m_generation(0)
{
}

const PresetProgram* PresetShaderCache::GetOrBuild(PresetShaderStage stage, const char* szPresetShaderText)
{
    if (szPresetShaderText == nullptr)
        szPresetShaderText = "";

    std::string key(1, stage == PresetShaderStage::Warp ? 'w' : 'c');
    key += szPresetShaderText;

    const auto found = m_entries.find(key);

    if (found != m_entries.end())
    {
        m_hits++;
        found->second.lastUsed = m_generation;
        return &found->second;
    }

    m_misses++;

    PresetProgram program;
    program.lastUsed = m_generation;
    const TranslatedShader translated = TranslatePresetShader(stage, szPresetShaderText);

    if (!translated.ok)
    {
        program.error = translated.error;
    }
    else
    {
        program.glsl = translated.glsl;
        std::unique_ptr<Shader> shader(new Shader());
        const ShaderBuildResult built = shader->BuildFromSource(PresetVertexSource(stage), program.glsl.c_str());

        if (built.ok)
            program.shader = std::move(shader);
        else
            program.error = std::string(StageName(stage)) + " shader compile/link failed: " + built.log;
    }

    return &m_entries.emplace(key, std::move(program)).first->second;
}

void PresetShaderCache::TrimUnused()
{
    for (auto it = m_entries.begin(); it != m_entries.end();)
    {
        if (m_generation - it->second.lastUsed < 2)
            ++it;
        else
            it = m_entries.erase(it);
    }
}

void PresetShaderCache::Clear()
{
    m_entries.clear();
}

}
