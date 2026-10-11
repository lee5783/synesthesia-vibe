//
//  hlsl-survey.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#include "Engine.h"
#include "HLSLParser.h"
#include "HLSLTree.h"
#include "GLSLGenerator.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace M4
{
extern thread_local std::string g_lastLogError;
}

namespace
{

namespace fs = std::filesystem;

constexpr char kLcc = 1;

std::string ReadFileRaw(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string ReadFileConvertingLineEndings(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::string out;
    char prev = 0;
    char ch;
    while (in.get(ch))
    {
        char c = ch;
        if (c == 10)
        {
            if (prev == 13)
            {
                prev = ch;
                continue;
            }
            c = kLcc;
        }
        else if (c == 13)
        {
            c = kLcc;
        }
        out.push_back(c);
        prev = ch;
    }
    return out;
}

void StripCommentsBuffer(char* str)
{
    if (!str || !str[0] || !str[1])
        return;

    char c0 = str[0];
    char c1 = str[1];
    char* dest = str;
    char* p = &str[1];
    bool bIgnoreTilEndOfLine = false;
    bool bIgnoreTilCloseComment = false;
    int nCharsToSkip = 0;
    while (true)
    {
        if (!bIgnoreTilCloseComment && c0 == '/' && c1 == '/')
            bIgnoreTilEndOfLine = true;
        if (bIgnoreTilEndOfLine && (c0 == 10 || c0 == 13))
        {
            bIgnoreTilEndOfLine = false;
            nCharsToSkip = 0;
        }
        if (!bIgnoreTilEndOfLine && c0 == '/' && c1 == '*')
            bIgnoreTilCloseComment = true;
        if (bIgnoreTilCloseComment && c0 == '*' && c1 == '/')
        {
            bIgnoreTilCloseComment = false;
            nCharsToSkip = 2;
        }
        if (!bIgnoreTilEndOfLine && !bIgnoreTilCloseComment)
        {
            if (nCharsToSkip > 0)
                nCharsToSkip--;
            else
                *dest++ = c0;
        }
        if (c1 == 0)
            break;
        p++;
        c0 = c1;
        c1 = *p;
    }
    *dest++ = 0;
}

std::string StripComments(const std::string& in)
{
    std::vector<char> buf(in.begin(), in.end());
    buf.push_back(0);
    StripCommentsBuffer(buf.data());
    return std::string(buf.data());
}

std::string ConvertLccToCrlf(const std::string& s)
{
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s)
    {
        if (c == kLcc)
        {
            out.push_back(13);
            out.push_back(10);
        }
        else
        {
            out.push_back(c);
        }
    }
    return out;
}

std::string DropUnsupportedTextureBinding(const std::string& text)
{
    static const std::regex kPattern(R"(=\s*sampler_state\s*\{\s*Texture\s*=\s*<[^>]*>\s*;\s*\}\s*;)");
    return std::regex_replace(text, kPattern, ";");
}

using KeyValues = std::map<std::string, std::string>;

KeyValues ParsePresetKeyValues(const std::string& raw)
{
    KeyValues kv;
    size_t i = 0;
    const size_t n = raw.size();
    while (i < n)
    {
        size_t lineEnd = raw.find_first_of("\r\n", i);
        if (lineEnd == std::string::npos)
            lineEnd = n;
        std::string line = raw.substr(i, lineEnd - i);
        size_t split = line.find_first_of("= ");
        if (split != std::string::npos)
        {
            std::string key = line.substr(0, split);
            std::string value = line.substr(split + 1);
            kv.emplace(key, value);
        }
        i = lineEnd;
        while (i < n && (raw[i] == '\r' || raw[i] == '\n'))
            i++;
    }
    return kv;
}

int GetInt(const KeyValues& kv, const std::string& key, int def)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return def;
    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        return def;
    }
}

float GetFloat(const KeyValues& kv, const std::string& key, float def)
{
    auto it = kv.find(key);
    if (it == kv.end())
        return def;
    try
    {
        return std::stof(it->second);
    }
    catch (...)
    {
        return def;
    }
}

std::string ReadCodeLines(const KeyValues& kv, const std::string& prefix)
{
    std::string out;
    int line = 1;
    while (true)
    {
        auto it = kv.find(prefix + std::to_string(line));
        if (it == kv.end())
            break;
        std::string value = it->second;
        if (!value.empty() && value[0] == '`')
            value.erase(0, 1);
        out += value;
        out.push_back(kLcc);
        line++;
    }
    return out;
}

std::string Formatted(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

std::string GenDefaultWarpShaderText(const std::string& defaultText, float decay, bool wrap)
{
    size_t bracePos = defaultText.find_last_of('{');
    if (bracePos == std::string::npos)
        return defaultText;
    std::string out = defaultText.substr(0, bracePos + 1);
    out += Formatted("    // sample previous frame%c", kLcc);
    out += Formatted("    ret = tex2D( sampler%s_main, uv ).xyz;%c", wrap ? "" : "_fc", kLcc);
    out += Formatted("    %c", kLcc);
    out += Formatted("    // darken (decay) over time%c", kLcc);
    out += Formatted("    ret *= %.2f; //or try: ret -= 0.004;%c", decay, kLcc);
    out += Formatted("}%c", kLcc);
    return out;
}

std::string GenDefaultCompShaderText(const std::string& defaultText, float brightness, float veAlpha,
                                      float veZoom, int veOrient, float hueShader, bool brighten,
                                      bool darken, bool solarize, bool invert)
{
    size_t bracePos = defaultText.find_last_of('{');
    if (bracePos == std::string::npos)
        return defaultText;
    std::string out = defaultText.substr(0, bracePos + 1);

    if (veAlpha > 0.001f)
    {
        int orientX = (veOrient % 2) ? -1 : 1;
        int orientY = (veOrient >= 2) ? -1 : 1;
        out += Formatted("    float2 uv_echo = (uv - 0.5)*%.3f*float2(%d,%d) + 0.5;%c",
                          1.0f / veZoom, orientX, orientY, kLcc);
        out += Formatted("    ret = lerp( tex2D(sampler_main, uv).xyz, %c", kLcc);
        out += Formatted("                tex2D(sampler_main, uv_echo).xyz, %c", kLcc);
        out += Formatted("                %.2f %c", veAlpha, kLcc);
        out += Formatted("              ); //video echo%c", kLcc);
        out += Formatted("    ret *= %.2f; //gamma%c", brightness, kLcc);
    }
    else
    {
        out += Formatted("    ret = tex2D(sampler_main, uv).xyz;%c", kLcc);
        out += Formatted("    ret *= %.2f; //gamma%c", brightness, kLcc);
    }

    if (hueShader >= 1.0f)
        out += Formatted("    ret *= hue_shader; //old hue shader effect%c", kLcc);
    else if (hueShader > 0.001f)
        out += Formatted("    ret *= %.2f + %.2f*hue_shader; //old hue shader effect%c", 1 - hueShader,
                          hueShader, kLcc);

    if (brighten)
        out += Formatted("    ret = 1 - (1-ret) * (1-ret); //brighten%c", kLcc);
    if (darken)
        out += Formatted("    ret *= ret; //darken%c", kLcc);
    if (solarize)
        out += Formatted("    ret = ret*(1-ret)*2; //solarize%c", kLcc);
    if (invert)
        out += Formatted("    ret = 1 - ret; //invert%c", kLcc);

    out += Formatted("}%c", kLcc);
    return out;
}

struct Assembly
{
    bool ok = false;
    std::string text;
    std::string error;
};

Assembly AssembleShaderText(const std::string& includeText, const std::string& shaderTextLcc, bool isWarp)
{
    static const char* kWarpDefines = "#define rad _rad_ang.x\n"
                                       "#define ang _rad_ang.y\n"
                                       "#define uv _uv.xy\n"
                                       "#define uv_orig _uv.zw\n";
    static const char* kCompDefines = "#define rad _rad_ang.x\n"
                                       "#define ang _rad_ang.y\n"
                                       "#define uv _uv.xy\n"
                                       "#define uv_orig _uv.xy\n"
                                       "#define hue_shader _vDiffuse.xyz\n";
    static const char* kWarpParams = "float4 _vDiffuse : COLOR, float4 _uv : TEXCOORD0, "
                                      "float2 _rad_ang : TEXCOORD1, out float4 _return_value : COLOR0";
    static const char* kCompParams = "float4 _vDiffuse : COLOR, float2 _uv : TEXCOORD0, "
                                      "float2 _rad_ang : TEXCOORD1, out float4 _return_value : COLOR0";
    static const char* kFirstLine = "    float3 ret = 0;";
    static const char* kLastLine = "    _return_value = float4(ret.xyz, _vDiffuse.w);";

    std::string prelude = includeText + (isWarp ? kWarpDefines : kCompDefines);

    std::string body = StripComments(ConvertLccToCrlf(shaderTextLcc));

    size_t markerPos = body.find("shader_body");
    if (markerPos == std::string::npos)
        return { false, {}, "no 'shader_body' marker found in the preset shader text" };

    body.replace(markerPos, 11, std::string(11, ' '));
    size_t insertPos = markerPos + 11;
    std::string funcDecl = std::string("void PS( ") + (isWarp ? kWarpParams : kCompParams) + " )\n";
    body.insert(insertPos, funcDecl);

    size_t bracePos = body.find('{', insertPos + funcDecl.size());
    if (bracePos == std::string::npos)
        return { false, {}, "no opening brace found after the shader_body marker" };
    body.insert(bracePos + 1, std::string(kFirstLine) + "\n");

    size_t lastBrace = body.find_last_of('}');
    if (lastBrace == std::string::npos)
        return { false, {}, "no closing brace found in the shader body" };
    body = body.substr(0, lastBrace) + " " + kLastLine + "\n}\n";

    return { true, prelude + body, {} };
}

enum class ShaderVerdict
{
    NotAssembled,
    ParseFailed,
    GenerateFailed,
    Ok,
};

struct ShaderRecord
{
    std::string presetName;
    std::string kind;
    ShaderVerdict verdict = ShaderVerdict::NotAssembled;
    std::string firstError;
};

ShaderRecord RunOne(const std::string& presetName, const std::string& kind, const Assembly& assembly)
{
    ShaderRecord rec;
    rec.presetName = presetName;
    rec.kind = kind;

    if (!assembly.ok)
    {
        rec.verdict = ShaderVerdict::NotAssembled;
        rec.firstError = assembly.error;
        return rec;
    }

    M4::g_lastLogError.clear();
    M4::Allocator allocator;
    M4::HLSLTree tree(&allocator);
    M4::HLSLParser parser(&allocator, &tree);
    std::string shaderId = presetName + "/" + kind;

    std::string preprocessed;
    bool preprocessOk = parser.ApplyPreprocessor(shaderId.c_str(), assembly.text.c_str(), assembly.text.size(),
                                                  preprocessed);
    if (!preprocessOk)
    {
        rec.verdict = ShaderVerdict::ParseFailed;
        rec.firstError = M4::g_lastLogError.empty() ? "preprocessing failed (no error text captured)"
                                                     : M4::g_lastLogError;
        return rec;
    }

    bool parseOk = parser.Parse(shaderId.c_str(), preprocessed.c_str(), preprocessed.size());
    if (!parseOk)
    {
        rec.verdict = ShaderVerdict::ParseFailed;
        rec.firstError = M4::g_lastLogError.empty() ? "parse failed (no error text captured)" : M4::g_lastLogError;
        return rec;
    }

    M4::g_lastLogError.clear();
    M4::GLSLGenerator generator;
    bool genOk = generator.Generate(&tree, M4::GLSLGenerator::Target_FragmentShader,
                                     M4::GLSLGenerator::Version_330, "PS");
    if (!genOk)
    {
        rec.verdict = ShaderVerdict::GenerateFailed;
        rec.firstError = M4::g_lastLogError.empty() ? "generate failed (no error text captured)" : M4::g_lastLogError;
        return rec;
    }

    rec.verdict = ShaderVerdict::Ok;
    return rec;
}


std::string TrimTrailingWhitespace(const std::string& s)
{
    size_t end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? std::string() : s.substr(0, end + 1);
}

ShaderRecord Finalize(ShaderRecord rec)
{
    rec.firstError = TrimTrailingWhitespace(rec.firstError);
    return rec;
}

std::string CategoryOf(const std::string& error)
{
    if (error.empty())
        return "(none)";
    size_t colon = error.find(':');
    std::string head = (colon != std::string::npos) ? error.substr(0, colon) : error;
    size_t paren = head.find('(');
    if (paren != std::string::npos)
        head = head.substr(0, paren);
    while (!head.empty() && (head.back() == ' ' || head.back() == '\t'))
        head.pop_back();
    return head.empty() ? error.substr(0, 60) : head;
}

} // namespace

int main(int argc, char** argv)
{
    std::string repoRoot = ".";
    std::string outPath = "hlsl-survey-report.md";
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--repo-root" && i + 1 < argc)
            repoRoot = argv[++i];
        else if (arg == "--out" && i + 1 < argc)
            outPath = argv[++i];
    }

    fs::path dataDir = fs::path(repoRoot) / "ref-src" / "beatdrop" / "BeatDrop" / "BeatDrop Resources" / "data";
    fs::path presetsDir = fs::path(repoRoot) / "ref-src" / "beatdrop" / "BeatDrop" / "BeatDrop Resources" / "presets";

    if (!fs::exists(presetsDir))
    {
        std::cerr << "Presets directory not found: " << presetsDir << std::endl;
        return 1;
    }

    std::string includeText = DropUnsupportedTextureBinding(StripComments(ReadFileRaw(dataDir / "include.fx")));
    if (includeText.empty())
    {
        std::cerr << "Failed to read " << (dataDir / "include.fx") << std::endl;
        return 1;
    }
    std::string defaultWarpText = ReadFileConvertingLineEndings(dataDir / "warp_ps.fx");
    std::string defaultCompText = ReadFileConvertingLineEndings(dataDir / "comp_ps.fx");
    if (defaultWarpText.empty() || defaultCompText.empty())
    {
        std::cerr << "Failed to read warp_ps.fx / comp_ps.fx from " << dataDir << std::endl;
        return 1;
    }

    std::vector<fs::path> presetFiles;
    for (const auto& entry : fs::directory_iterator(presetsDir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".milk")
            presetFiles.push_back(entry.path());
    }
    std::sort(presetFiles.begin(), presetFiles.end());

    std::vector<ShaderRecord> records;
    int totalPresets = static_cast<int>(presetFiles.size());
    int skippedBelow200 = 0;
    int surveyed = 0;

    for (const auto& path : presetFiles)
    {
        std::string presetName = path.filename().string();
        std::string raw = ReadFileRaw(path);
        KeyValues kv = ParsePresetKeyValues(raw);

        int presetVer = GetInt(kv, "MILKDROP_PRESET_VERSION", 100);
        if (presetVer < 200)
        {
            skippedBelow200++;
            continue;
        }
        surveyed++;

        int warpVer, compVer;
        if (presetVer == 200)
        {
            warpVer = compVer = GetInt(kv, "PSVERSION", 2);
        }
        else
        {
            warpVer = GetInt(kv, "PSVERSION_WARP", 2);
            compVer = GetInt(kv, "PSVERSION_COMP", 2);
        }
        (void)warpVer;
        (void)compVer;

        std::string warpLcc = ReadCodeLines(kv, "warp_");
        if (warpLcc.empty())
        {
            float decay = GetFloat(kv, "fDecay", 0.98f);
            bool wrap = GetInt(kv, "bTexWrap", 1) != 0;
            warpLcc = GenDefaultWarpShaderText(defaultWarpText, decay, wrap);
        }

        std::string compLcc = ReadCodeLines(kv, "comp_");
        if (compLcc.empty())
        {
            float brightness = GetFloat(kv, "fGammaAdj", 2.0f);
            float veAlpha = GetFloat(kv, "fVideoEchoAlpha", 0.0f);
            float veZoom = GetFloat(kv, "fVideoEchoZoom", 2.0f);
            int veOrient = GetInt(kv, "nVideoEchoOrientation", 0);
            float hueShader = GetFloat(kv, "fShader", 0.0f);
            bool brighten = GetInt(kv, "bBrighten", 0) != 0;
            bool darken = GetInt(kv, "bDarken", 0) != 0;
            bool solarize = GetInt(kv, "bSolarize", 0) != 0;
            bool invert = GetInt(kv, "bInvert", 0) != 0;
            compLcc = GenDefaultCompShaderText(defaultCompText, brightness, veAlpha, veZoom, veOrient,
                                                hueShader, brighten, darken, solarize, invert);
        }

        Assembly warpAsm = AssembleShaderText(includeText, warpLcc, true);
        Assembly compAsm = AssembleShaderText(includeText, compLcc, false);

        records.push_back(Finalize(RunOne(presetName, "warp", warpAsm)));
        records.push_back(Finalize(RunOne(presetName, "comp", compAsm)));
    }

    int notAssembled = 0, parseFailed = 0, generateFailed = 0, ok = 0;
    std::map<std::string, int> failureCategories;
    for (const auto& r : records)
    {
        switch (r.verdict)
        {
        case ShaderVerdict::NotAssembled:
            notAssembled++;
            failureCategories[CategoryOf(r.firstError)]++;
            break;
        case ShaderVerdict::ParseFailed:
            parseFailed++;
            failureCategories[CategoryOf(r.firstError)]++;
            break;
        case ShaderVerdict::GenerateFailed:
            generateFailed++;
            failureCategories[CategoryOf(r.firstError)]++;
            break;
        case ShaderVerdict::Ok:
            ok++;
            break;
        }
    }

    static const char* kMvpPresets[3] = {
        "Eo.S. + Geiss - glowsticks v2 02 (Relief Mix).milk",
        "Goody - Need - Transcendance remix.milk",
        "Stahlregen & Flexi + Geiss - Tiger No.5 (Random texture flow).milk",
    };

    std::ofstream out(outPath);
    out << "# P2-004 hlsl-survey report\n\n";
    out << "Corpus: `" << presetsDir.string() << "`\n\n";
    out << "- Total `.milk` presets found: " << totalPresets << " (BeatDrop v1.5 ships 574)\n";
    out << "- Skipped (`MILKDROP_PRESET_VERSION` < 200, fixed-function only): " << skippedBelow200 << "\n";
    out << "- Surveyed (presetver >= 200): " << surveyed << "\n";
    out << "- Shaders attempted (warp + comp per surveyed preset): " << records.size() << "\n\n";

    out << "## Totals\n\n";
    out << "| Outcome | Count |\n|---|---|\n";
    out << "| Parsed and generated OK | " << ok << " |\n";
    out << "| Assembly failed (before HLSLParser) | " << notAssembled << " |\n";
    out << "| HLSLParser::Parse failed | " << parseFailed << " |\n";
    out << "| GLSLGenerator::Generate failed | " << generateFailed << " |\n\n";

    out << "## Failure categories (first error, grouped)\n\n";
    out << "| Category | Count |\n|---|---|\n";
    for (const auto& [category, count] : failureCategories)
        out << "| " << category << " | " << count << " |\n";
    out << "\n";

    out << "## MVP presetver-201 presets (individual, warp and comp)\n\n";
    out << "| Preset | Shader | Result | First error |\n|---|---|---|---|\n";
    for (const char* mvpName : kMvpPresets)
    {
        for (const auto& r : records)
        {
            if (r.presetName != mvpName)
                continue;
            const char* verdictStr = "OK";
            switch (r.verdict)
            {
            case ShaderVerdict::NotAssembled: verdictStr = "ASSEMBLY FAIL"; break;
            case ShaderVerdict::ParseFailed: verdictStr = "PARSE FAIL"; break;
            case ShaderVerdict::GenerateFailed: verdictStr = "GENERATE FAIL"; break;
            case ShaderVerdict::Ok: verdictStr = "OK"; break;
            }
            out << "| " << r.presetName << " | " << r.kind << " | " << verdictStr << " | "
                << (r.firstError.empty() ? "" : r.firstError) << " |\n";
        }
    }
    out << "\n";

    out << "## All failures (preset, shader, first error)\n\n";
    out << "| Preset | Shader | Result | First error |\n|---|---|---|---|\n";
    for (const auto& r : records)
    {
        if (r.verdict == ShaderVerdict::Ok)
            continue;
        const char* verdictStr = (r.verdict == ShaderVerdict::NotAssembled) ? "ASSEMBLY FAIL"
                                  : (r.verdict == ShaderVerdict::ParseFailed) ? "PARSE FAIL"
                                                                              : "GENERATE FAIL";
        out << "| " << r.presetName << " | " << r.kind << " | " << verdictStr << " | " << r.firstError << " |\n";
    }

    out.close();

    std::cout << "hlsl-survey: " << totalPresets << " presets, " << surveyed << " surveyed ("
              << skippedBelow200 << " skipped), " << ok << "/" << records.size() << " shaders OK.\n";
    std::cout << "Report written to " << outPath << std::endl;

    return 0;
}
