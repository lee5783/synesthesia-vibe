//
//  main.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "gl_headers.h"

#include "bindings_check.h"
#include "shader_translator.h"

namespace
{
    namespace fs = std::filesystem;

    constexpr char kLineFeedControlChar = 1;

    const char* UsageLine()
    {
        return "usage: shader-translate-check --presets <dir> --out <dir> [--target gl|es] [--resources <dir> --golden <dir>]";
    }

    std::string ReadFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::map<std::string, std::string> ParseKeyValues(const std::string& raw)
    {
        std::map<std::string, std::string> values;
        size_t index = 0;

        while (index < raw.size())
        {
            size_t lineEnd = raw.find_first_of("\r\n", index);

            if (lineEnd == std::string::npos)
                lineEnd = raw.size();

            const std::string line = raw.substr(index, lineEnd - index);
            const size_t split = line.find_first_of("= ");

            if (split != std::string::npos)
                values.emplace(line.substr(0, split), line.substr(split + 1));

            index = lineEnd;

            while (index < raw.size() && (raw[index] == '\r' || raw[index] == '\n'))
                ++index;
        }

        return values;
    }

    std::string ReadCodeLines(const std::map<std::string, std::string>& values, const std::string& prefix)
    {
        std::string text;

        for (int line = 1;; ++line)
        {
            const auto found = values.find(prefix + std::to_string(line));

            if (found == values.end())
                break;

            std::string value = found->second;

            if (!value.empty() && value[0] == '`')
                value.erase(0, 1);

            text += value;
            text.push_back(kLineFeedControlChar);
        }

        return text;
    }

    std::string SafeName(const std::string& name)
    {
        std::string safe;

        for (char c : name)
            safe.push_back(isalnum(static_cast<unsigned char>(c)) ? c : '-');

        return safe;
    }

    struct Tally
    {
        int attempted = 0;
        int translated = 0;
        int linked = 0;
        int failed = 0;
    };

    void CheckShader(render_gl::PresetShaderCache& cache, render_gl::PresetShaderStage stage, const std::string& presetName,
        const char* szKind, const std::string& text, const fs::path& outDir, Tally& tally)
    {
        ++tally.attempted;

        const render_gl::PresetProgram* program = cache.GetOrBuild(stage, text.c_str());

        if (!program->glsl.empty())
        {
            ++tally.translated;
            std::ofstream glslFile(outDir / (SafeName(presetName) + "-" + szKind + ".frag"), std::ios::out | std::ios::trunc | std::ios::binary);
            glslFile << program->glsl;
        }

        if (program->shader != nullptr && program->shader->Valid())
        {
            ++tally.linked;
            printf("OK    %-70s %s\n", presetName.c_str(), szKind);
        }
        else
        {
            ++tally.failed;
            printf("FAIL  %-70s %s\n      %s\n", presetName.c_str(), szKind, program->error.c_str());
        }
    }

    std::vector<fs::path> MilkFiles(const std::string& presetsDir)
    {
        std::vector<fs::path> files;

        for (const auto& entry : fs::directory_iterator(presetsDir))
        {
            if (entry.path().extension() == ".milk")
                files.push_back(entry.path());
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    void TranslateEsShader(render_gl::PresetShaderStage stage, const std::string& presetName, const char* szKind,
        const std::string& text, const fs::path& outDir, Tally& tally)
    {
        ++tally.attempted;

        const render_gl::TranslatedShader translated = render_gl::TranslatePresetShaderFor(render_gl::GlslTarget::Es300, stage, text.c_str());

        if (!translated.ok)
        {
            ++tally.failed;
            printf("FAIL  %-70s %s\n      %s\n", presetName.c_str(), szKind, translated.error.c_str());
            return;
        }

        ++tally.translated;
        std::ofstream glslFile(outDir / (SafeName(presetName) + "-" + szKind + ".frag"), std::ios::out | std::ios::trunc | std::ios::binary);
        glslFile << translated.glsl;
        printf("OK    %-70s %s linked=n/a\n", presetName.c_str(), szKind);
    }

    int RunEsTranslation(const std::string& presetsDir, const fs::path& outDir)
    {
        Tally tally;

        for (const fs::path& file : MilkFiles(presetsDir))
        {
            const std::map<std::string, std::string> values = ParseKeyValues(ReadFile(file));
            const auto version = values.find("MILKDROP_PRESET_VERSION");
            const int presetVersion = version == values.end() ? 0 : atoi(version->second.c_str());
            const std::string name = file.stem().string();

            if (presetVersion < 200)
            {
                printf("SKIP  %-70s presetver %d (no HLSL)\n", name.c_str(), presetVersion);
                continue;
            }

            const std::string warp = ReadCodeLines(values, "warp_");
            const std::string comp = ReadCodeLines(values, "comp_");

            if (!warp.empty())
                TranslateEsShader(render_gl::PresetShaderStage::Warp, name, "warp", warp, outDir, tally);

            if (!comp.empty())
                TranslateEsShader(render_gl::PresetShaderStage::Composite, name, "comp", comp, outDir, tally);
        }

        printf("\nSUMMARY target=es attempted=%d translated=%d linked=n/a failed=%d\n", tally.attempted, tally.translated, tally.failed);

        const int exitCode = tally.failed != 0 || tally.attempted == 0 ? EXIT_FAILURE : EXIT_SUCCESS;
        printf("EXIT %d\n", exitCode);
        return exitCode;
    }

    int CheckBrokenShaders(render_gl::PresetShaderCache& cache)
    {
        const char* const brokenSamples[] = {
            "shader_body\n{\n    ret = tex2D(sampler_main, uv).xyz * ;\n}\n",
            "shader_body\n{\n    ret = undefined_function(uv);\n}\n",
            "no entry marker here",
            "",
        };

        int failures = 0;
        int sampleIndex = 0;

        for (const char* sample : brokenSamples)
        {
            std::string text(sample);

            std::replace(text.begin(), text.end(), '\n', kLineFeedControlChar);

            const render_gl::PresetProgram* program = cache.GetOrBuild(render_gl::PresetShaderStage::Composite, text.c_str());
            const bool bFellBack = program->shader == nullptr;

            printf("BROKEN[%d] fallback=%s error=%s\n", sampleIndex, bFellBack ? "yes" : "NO", program->error.c_str());

            if (!bFellBack || program->error.empty())
                ++failures;

            ++sampleIndex;
        }

        return failures;
    }
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);

    std::string presetsDir;
    std::string outDir;
    std::string resourcesDir;
    std::string goldenDir;
    std::string target = "gl";

    for (int index = 1; index < argc; ++index)
    {
        const char* arg = argv[index];

        if (strcmp(arg, "--presets") == 0 && index + 1 < argc)
        {
            presetsDir = argv[++index];
        }
        else if (strcmp(arg, "--out") == 0 && index + 1 < argc)
        {
            outDir = argv[++index];
        }
        else if (strcmp(arg, "--resources") == 0 && index + 1 < argc)
        {
            resourcesDir = argv[++index];
        }
        else if (strcmp(arg, "--golden") == 0 && index + 1 < argc)
        {
            goldenDir = argv[++index];
        }
        else if (strcmp(arg, "--target") == 0 && index + 1 < argc)
        {
            target = argv[++index];
        }
        else
        {
            fprintf(stderr, "shader-translate-check: unrecognized argument \"%s\"\n%s\n", arg, UsageLine());
            return 2;
        }
    }

    if (presetsDir.empty() || outDir.empty() || (target != "gl" && target != "es"))
    {
        fprintf(stderr, "%s\n", UsageLine());
        return 2;
    }

    std::error_code dirError;
    fs::create_directories(outDir, dirError);

    if (dirError)
    {
        fprintf(stderr, "shader-translate-check: cannot create \"%s\"\n", outDir.c_str());
        return 2;
    }

    if (target == "es")
        return RunEsTranslation(presetsDir, fs::path(outDir));

    if (glfwInit() != GLFW_TRUE)
    {
        fprintf(stderr, "shader-translate-check: glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(64, 64, "shader-translate-check", nullptr, nullptr);

    if (window == nullptr)
    {
        fprintf(stderr, "shader-translate-check: glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (gladLoadGL(glfwGetProcAddress) == 0)
    {
        fprintf(stderr, "shader-translate-check: gladLoadGL failed\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    printf("GL_RENDERER: %s\n", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    printf("GL_VERSION: %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    printf("GL_SHADING_LANGUAGE_VERSION: %s\n", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

    int exitCode = EXIT_SUCCESS;

    {
        render_gl::PresetShaderCache cache;
        Tally tally;
        std::vector<std::string> warpTexts;
        std::vector<PresetShaderTexts> shaderPresets;

        for (const fs::path& file : MilkFiles(presetsDir))
        {
            const std::map<std::string, std::string> values = ParseKeyValues(ReadFile(file));
            const auto version = values.find("MILKDROP_PRESET_VERSION");
            const int presetVersion = version == values.end() ? 0 : atoi(version->second.c_str());
            const std::string name = file.stem().string();

            if (presetVersion < 200)
            {
                printf("SKIP  %-70s presetver %d (no HLSL)\n", name.c_str(), presetVersion);
                continue;
            }

            const std::string warp = ReadCodeLines(values, "warp_");
            const std::string comp = ReadCodeLines(values, "comp_");

            if (!warp.empty())
            {
                CheckShader(cache, render_gl::PresetShaderStage::Warp, name, "warp", warp, fs::path(outDir), tally);
                warpTexts.push_back(warp);
            }

            if (!comp.empty())
                CheckShader(cache, render_gl::PresetShaderStage::Composite, name, "comp", comp, fs::path(outDir), tally);

            shaderPresets.push_back(PresetShaderTexts{ name, warp, comp });
        }

        printf("\nSUMMARY attempted=%d translated=%d linked=%d failed=%d\n", tally.attempted, tally.translated, tally.linked, tally.failed);

        if (tally.failed != 0 || tally.attempted == 0)
            exitCode = EXIT_FAILURE;

        printf("CACHE before repeat: entries=%zu hits=%d misses=%d\n", cache.Size(), cache.HitCount(), cache.MissCount());

        for (const std::string& warp : warpTexts)
            cache.GetOrBuild(render_gl::PresetShaderStage::Warp, warp.c_str());

        printf("CACHE after repeat of %zu warp lookups: entries=%zu hits=%d misses=%d\n", warpTexts.size(), cache.Size(), cache.HitCount(), cache.MissCount());

        if (cache.HitCount() != static_cast<int>(warpTexts.size()))
            exitCode = EXIT_FAILURE;

        if (CheckBrokenShaders(cache) != 0)
            exitCode = EXIT_FAILURE;

        if (!resourcesDir.empty() && !goldenDir.empty())
        {
            if (RunBindingsCheck(shaderPresets, resourcesDir, goldenDir, cache) != 0)
                exitCode = EXIT_FAILURE;
        }

        cache.Clear();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    printf("EXIT %d\n", exitCode);
    return exitCode;
}
