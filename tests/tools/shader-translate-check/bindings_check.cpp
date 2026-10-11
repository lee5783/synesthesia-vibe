//
//  bindings_check.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-09.
//

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <random>
#include <set>
#include <sstream>

#include "gl_headers.h"

#include "audio_textures.h"
#include "bindings_check.h"
#include "blur_chain.h"
#include "disk_textures.h"
#include "noise_textures.h"
#include "preset_shader_inputs.h"
#include "state.h"

namespace
{
    namespace fs = std::filesystem;

    constexpr unsigned kDeterministicSeed = 20260927;
    constexpr int kBindingsTexSize = 848;

    class ZeroClock : public render_core::IRawClock
    {
    public:
        bool QueryHighPrecisionTicks(long long& outTicks) override
        {
            outTicks = 0;
            return true;
        }

        unsigned QueryLowPrecisionMilliseconds() override
        {
            return 0;
        }
    };

    struct DumpConstant
    {
        std::string name;
        int registerIndex;
        bool isSampler;
    };

    struct DumpPass
    {
        std::string label;
        std::vector<DumpConstant> constants;
    };

    struct BindingsTally
    {
        int programs = 0;
        int exactMatches = 0;
        int samplerOrderMismatches = 0;
        int warnings = 0;
    };

    std::string ReadFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::vector<DumpPass> LoadDumpPasses(const fs::path& goldenDir)
    {
        std::vector<DumpPass> passes;

        for (const auto& presetEntry : fs::directory_iterator(goldenDir))
        {
            const fs::path dumps = presetEntry.path() / "dumps";

            if (!presetEntry.is_directory() || !fs::exists(dumps))
                continue;

            for (const auto& file : fs::directory_iterator(dumps))
            {
                const std::string fileName = file.path().filename().string();

                if (fileName.rfind("pass_", 0) != 0 || file.path().extension() != ".json")
                    continue;

                DumpPass pass;
                pass.label = presetEntry.path().filename().string() + "/" + fileName;

                std::istringstream lines(ReadFile(file.path()));
                std::string line;

                while (std::getline(lines, line))
                {
                    const std::string nameKeyText = "\"name\": \"";
                    const size_t nameKey = line.find(nameKeyText);

                    if (nameKey == std::string::npos)
                        continue;

                    const size_t nameStart = nameKey + nameKeyText.size();
                    const size_t nameEnd = line.find('"', nameStart);
                    const std::string registerKeyText = "\"registerIndex\": ";
                    const size_t registerKey = line.find(registerKeyText);

                    DumpConstant constant;
                    constant.name = line.substr(nameStart, nameEnd - nameStart);
                    constant.registerIndex = registerKey == std::string::npos ? -1 : atoi(line.c_str() + registerKey + registerKeyText.size());
                    constant.isSampler = line.find("\"isSampler\": true") != std::string::npos;
                    pass.constants.push_back(constant);
                }

                passes.push_back(pass);
            }
        }

        return passes;
    }

    void CompareWithDump(const render_gl::PresetShaderInputs& inputs, const std::vector<DumpPass>& passes, const std::string& title, BindingsTally& tally)
    {
        std::set<std::string> glNames(inputs.ActiveNames().begin(), inputs.ActiveNames().end());

        const DumpPass* best = nullptr;
        size_t bestDistance = static_cast<size_t>(-1);

        for (const DumpPass& pass : passes)
        {
            if (pass.constants.empty())
                continue;

            std::set<std::string> dumpNames;

            for (const DumpConstant& constant : pass.constants)
                dumpNames.insert(constant.name);

            size_t distance = 0;

            for (const std::string& name : glNames)
                distance += dumpNames.count(name) == 0 ? 1 : 0;

            for (const std::string& name : dumpNames)
                distance += glNames.count(name) == 0 ? 1 : 0;

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = &pass;
            }
        }

        ++tally.programs;

        if (best == nullptr)
        {
            printf("COMPARE %s: no dump pass found\n", title.c_str());
            return;
        }

        std::set<std::string> dumpNames;

        for (const DumpConstant& constant : best->constants)
            dumpNames.insert(constant.name);

        printf("COMPARE %s vs %s: nameSetDistance=%zu\n", title.c_str(), best->label.c_str(), bestDistance);

        for (const std::string& name : glNames)
        {
            if (dumpNames.count(name) == 0)
                printf("  only-in-GL   %s\n", name.c_str());
        }

        for (const std::string& name : dumpNames)
        {
            if (glNames.count(name) == 0)
                printf("  only-in-dump %s\n", name.c_str());
        }

        int orderMismatches = 0;
        int previousRegister = -1;

        for (const render_gl::SamplerBindingInfo& binding : inputs.Samplers())
        {
            for (const DumpConstant& constant : best->constants)
            {
                if (constant.name == binding.name && constant.isSampler)
                {
                    const bool bSame = constant.registerIndex == binding.unit;
                    const bool bAscending = constant.registerIndex > previousRegister;
                    printf("  sampler %-24s dumpRegister=%d glUnit=%d %s relativeOrder=%s\n", binding.name.c_str(), constant.registerIndex, binding.unit,
                        bSame ? "SAME" : "SHIFTED", bAscending ? "OK" : "DIFF");

                    if (!bAscending)
                        ++orderMismatches;

                    previousRegister = constant.registerIndex;
                }
            }
        }

        tally.samplerOrderMismatches += orderMismatches;

        if (bestDistance == 0 && orderMismatches == 0)
            ++tally.exactMatches;
    }

#if defined(_WIN32)
    std::string EmulateDx9Pick(const std::string& texturesDir, std::mt19937_64 gen)
    {
        std::vector<std::wstring> texfiles;
        const std::wstring mask = fs::path(texturesDir).wstring() + L"\\*.*";
        const wchar_t* const exts[] = { L"jpg", L"jpeg", L"jfif", L"dds", L"png", L"webp", L"tga", L"bmp", L"dib", L"gif" };

        WIN32_FIND_DATAW ffd = {};
        HANDLE hFind = FindFirstFileW(mask.c_str(), &ffd);

        if (hFind == INVALID_HANDLE_VALUE)
            return std::string();

        do
        {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            const wchar_t* ext = wcsrchr(ffd.cFileName, L'.');

            if (ext == nullptr)
                continue;

            for (const wchar_t* known : exts)
            {
                if (_wcsicmp(known, ext + 1) == 0)
                {
                    texfiles.push_back(ffd.cFileName);
                    break;
                }
            }
        }
        while (FindNextFileW(hFind, &ffd));

        FindClose(hFind);

        if (texfiles.empty())
            return std::string();

        std::uniform_int_distribution<size_t> dist(0, texfiles.size() - 1);
        const int i = static_cast<int>(dist(gen));
        return fs::path(texfiles[i]).string();
    }
#endif
}

int RunBindingsCheck(const std::vector<PresetShaderTexts>& presets, const std::string& resourcesDir, const std::string& goldenDir,
    render_gl::PresetShaderCache& cache)
{
    int failures = 0;
    const std::string texturesDir = (fs::path(resourcesDir) / "textures").string();
    const std::vector<DumpPass> passes = LoadDumpPasses(fs::path(goldenDir));

    printf("\nBINDINGS dump passes loaded=%zu texturesDir=%s\n", passes.size(), texturesDir.c_str());

    std::mt19937_64 gen(kDeterministicSeed);
    render_gl::NoiseTextures noise;
    ZeroClock clock;
    render_gl::NoiseVerifyOptions verify = { false, nullptr };

    if (!noise.Create(gen, clock, verify))
    {
        printf("BINDINGS FAIL noise create\n");
        return 1;
    }

    const std::mt19937_64 genAfterNoise = gen;

    render_gl::BlurChain blur;
    render_gl::AudioTextures audio;
    render_gl::DiskTextureCache disk;
    render_gl::RandomTexturePicker picker;

    if (!blur.Create(kBindingsTexSize, kBindingsTexSize) || !audio.Create())
    {
        printf("BINDINGS FAIL blur/audio create\n");
        return 1;
    }

    disk.SetDirectories(texturesDir, std::string());

    BindingsTally tally;

    for (const PresetShaderTexts& preset : presets)
    {
        gen = genAfterNoise;
        picker.Configure(texturesDir, &gen);

        for (int stageIndex = 0; stageIndex < 2; ++stageIndex)
        {
            const char* szStage = stageIndex == 0 ? "warp" : "comp";
            const std::string& text = stageIndex == 0 ? preset.warp : preset.comp;

            if (text.empty())
                continue;

            const render_gl::PresetShaderStage stage = stageIndex == 0 ? render_gl::PresetShaderStage::Warp : render_gl::PresetShaderStage::Composite;
            const render_gl::PresetProgram* program = cache.GetOrBuild(stage, text.c_str());

            render_gl::PresetShaderResources resources = { &noise, &blur, &audio, &disk, &picker };
            render_gl::PresetShaderInputs inputs;
            const std::mt19937_64 genBefore = gen;

            if (!inputs.Resolve(*program, resources))
            {
                printf("BINDINGS FAIL %s %s: resolve\n", preset.name.c_str(), szStage);
                ++failures;
                continue;
            }

            printf("\nBINDINGS %s %s\n%s", preset.name.c_str(), szStage, inputs.Describe().c_str());
            tally.warnings += static_cast<int>(inputs.Warnings().size());

            CompareWithDump(inputs, passes, preset.name + " " + szStage, tally);

            {
                std::unique_ptr<CState> statePtr(new CState());
                CState& state = *statePtr;
                state.Default();
                td_mysounddata mysound = {};
                FrameContext ctx = { mysound, 10.0f, 30.0f, 300, 0.0f, 0.0f, 20.0f, kBindingsTexSize, kBindingsTexSize, false,
                    0.0f, 0.0f, 0.0f, 0.0f, false, 0, 2026.0f, 10.0f, 9.0f, 5.0f, 12.0f, 30.0f, 15.0f, 10.0f };

                int highestBlur = 0;
                render_gl::PresetFrameInput frame;
                frame.ctx = &ctx;
                frame.state = &state;
                frame.randFrame = render_core::ShaderVec4{ 0.1f, 0.2f, 0.3f, 0.4f };
                frame.texSizeX = kBindingsTexSize;
                frame.texSizeY = kBindingsTexSize;
                frame.mainTexture = blur.Level(0).ColorTexture().Id();
                frame.highestBlurTexUsed = &highestBlur;

                glGetError();
                inputs.Apply(frame);
                inputs.Restore();
                const GLenum applyError = glGetError();
                printf("APPLY %s %s glError=0x%x highestBlurTexUsed=%d\n", preset.name.c_str(), szStage, applyError, highestBlur);

                if (applyError != GL_NO_ERROR)
                    ++failures;
            }

            for (const render_gl::SamplerBindingInfo& binding : inputs.Samplers())
            {
                if (binding.name.rfind("sampler_rand", 0) != 0)
                    continue;

                const render_gl::DiskTexture* texture = disk.Acquire(binding.rootName.c_str());
                const std::string chosen = texture != nullptr ? fs::path(texture->path).filename().string() : std::string("(none)");
                printf("RANDPICK %s %s %s root=%s file=%s\n", preset.name.c_str(), szStage, binding.name.c_str(), binding.rootName.c_str(), chosen.c_str());

#if defined(_WIN32)
                const std::string dx9 = EmulateDx9Pick(texturesDir, genBefore);
                const bool bSame = fs::path(dx9).stem().string() == binding.rootName;
                printf("RANDPICK dx9-emulation (FindFirstFileW order, std::uniform_int_distribution, same generator state) file=%s %s\n", dx9.c_str(), bSame ? "MATCH" : "DIFF");

                if (!bSame)
                    ++failures;
#endif

                render_gl::RandomTexturePicker rawPicker;
                std::mt19937_64 rawGen = genBefore;
                rawPicker.Configure(texturesDir, &rawGen);
                rawPicker.SetSortedOrder(false);
                char rawName[260] = {};
                rawPicker.PickRandomTexture("", rawName, sizeof(rawName));
                printf("RANDPICK raw-directory-order pick=%s root=%s %s\n", rawName, binding.rootName.c_str(),
                    fs::path(rawName).stem().string() == binding.rootName ? "SAME" : "DIFFERENT");
            }

            inputs.Destroy();
        }
    }

    printf("\nBINDINGS SUMMARY programs=%d exactNameSetAndRegisterMatches=%d samplerOrderMismatches=%d warnings=%d failures=%d\n",
        tally.programs, tally.exactMatches, tally.samplerOrderMismatches, tally.warnings, failures);

    disk.Destroy();
    audio.Destroy();
    blur.Destroy();
    noise.Destroy();

    return failures;
}
