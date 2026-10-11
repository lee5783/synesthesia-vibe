//
//  main.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-07.
//

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "synesthesia-vibe/synesthesia-vibe.h"

namespace
{
    constexpr int kFrameCount = 10;
    constexpr int kSamplesPerFrame = 735;
    constexpr int kChannels = 2;
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kToneHz = 440.0;
    constexpr double kToneAmplitude = 0.5;
    constexpr double kSampleRate = 44100.0;
    constexpr double kFrameSeconds = 1.0 / 60.0;

    const char kPresetRelativePath[] = "resources/mvp-presets/shifter - swarm.milk";
    const char kResourceRootRelativePath[] = "ref-src/beatdrop/BeatDrop/BeatDrop Resources";
    const char kBadPresetRelativePath[] = "resources/mvp-presets/this-preset-does-not-exist.milk";
    const char kRendererUnavailable[] = "renderer unavailable";

    const char* BdStatusName(sv_status status)
    {
        switch (status)
        {
        case SV_OK: return "SV_OK";
        case SV_ERR_IO: return "SV_ERR_IO";
        case SV_ERR_PARSE: return "SV_ERR_PARSE";
        case SV_ERR_SHADER: return "SV_ERR_SHADER";
        case SV_ERR_GL: return "SV_ERR_GL";
        case SV_ERR_ARG: return "SV_ERR_ARG";
        }
        return "SV_ERR_UNKNOWN";
    }

    std::string BdDirectoryOf(const char* path)
    {
        if (path == nullptr)
            return std::string(".");

        std::string text(path);
        const size_t slash = text.find_last_of("\\/");
        if (slash == std::string::npos)
            return std::string(".");

        return text.substr(0, slash == 0 ? 1 : slash);
    }

    bool BdFileExists(const std::string& path)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error);
    }

    std::string BdFindRepoRoot(const std::string& startDir)
    {
        std::string dir(startDir);

        for (int depth = 0; depth < 12; ++depth)
        {
            if (BdFileExists(dir + "/" + kPresetRelativePath))
                return dir;

            const size_t slash = dir.find_last_of("\\/");
            if (slash == std::string::npos)
                break;

            dir = dir.substr(0, slash == 0 ? 1 : slash);
        }

        return std::string(".");
    }

    sv_config BdMakeConfig(const std::string& resourceRoot)
    {
        sv_config config;
        memset(&config, 0, sizeof(config));
        config.width = 640;
        config.height = 480;
        config.mesh_x = 64;
        config.mesh_y = 48;
        config.texsize_x = 640;
        config.texsize_y = 480;
        config.fps_hint = 60.0f;
        config.resource_path = resourceRoot.c_str();
        config.rng_seed = 1;
        config.deterministic = true;
        config.pin_date = true;
        config.year = 2000.0f;
        config.month = 1.0f;
        config.day = 1.0f;
        config.weekday = 6.0f;
        return config;
    }

    int BdRunFrames(sv_context* ctx, const std::string& outDir)
    {
        std::vector<int16_t> pcm(static_cast<size_t>(kSamplesPerFrame) * kChannels);
        double phase = 0.0;
        const double phaseStep = 2.0 * kPi * kToneHz / kSampleRate;

        for (int frame = 0; frame < kFrameCount; ++frame)
        {
            for (int n = 0; n < kSamplesPerFrame; ++n)
            {
                const int16_t sample = static_cast<int16_t>(sin(phase) * kToneAmplitude * 32767.0);
                phase += phaseStep;
                if (phase >= 2.0 * kPi)
                    phase -= 2.0 * kPi;

                for (int channel = 0; channel < kChannels; ++channel)
                    pcm[static_cast<size_t>(n) * kChannels + channel] = sample;
            }

            sv_push_pcm_s16(ctx, pcm.data(), kSamplesPerFrame, kChannels);
            sv_render_frame(ctx, kFrameSeconds, 0);

            char jsonPath[1024];
            snprintf(jsonPath, sizeof(jsonPath), "%s/frame_%04d.json", outDir.c_str(), frame);
            sv_dump_frame_state(ctx, jsonPath);

            printf("synesthesia-vibe-headless: frame %04d -> %s\n", frame, jsonPath);
        }

        return kFrameCount;
    }
}

int main(int argc, char** argv)
{
    const std::string exeDir = BdDirectoryOf(argc > 0 ? argv[0] : nullptr);
    const std::string repoRoot = BdFindRepoRoot(exeDir);
    const std::string outDir = argc > 1 ? argv[1] : (exeDir + "/frames");

    std::error_code dirError;
    std::filesystem::create_directories(outDir, dirError);
    if (dirError)
    {
        fprintf(stderr, "synesthesia-vibe-headless: cannot create frame directory \"%s\": %s\n",
            outDir.c_str(), dirError.message().c_str());
        return EXIT_FAILURE;
    }

    const std::string resourceRoot = repoRoot + "/" + kResourceRootRelativePath;
    const std::string presetPath = repoRoot + "/" + kPresetRelativePath;

    printf("synesthesia-vibe-headless: repo_root=\"%s\"\n", repoRoot.c_str());
    printf("synesthesia-vibe-headless: resource_path=\"%s\"\n", resourceRoot.c_str());
    printf("synesthesia-vibe-headless: frame_dir=\"%s\"\n", outDir.c_str());

    const sv_config config = BdMakeConfig(resourceRoot);

    sv_context* ctx = sv_create(&config);
    if (ctx == nullptr)
    {
        fprintf(stderr, "synesthesia-vibe-headless: sv_create returned NULL\n");
        return EXIT_FAILURE;
    }

    const char* createError = sv_last_error(ctx);
    printf("synesthesia-vibe-headless: no-gl sv_last_error=\"%s\"\n", createError);

    if (createError == nullptr || strstr(createError, kRendererUnavailable) == nullptr)
    {
        fprintf(stderr, "synesthesia-vibe-headless: FAIL sv_create without GL did not report \"%s\"\n", kRendererUnavailable);
        sv_destroy(ctx);
        return EXIT_FAILURE;
    }

    printf("synesthesia-vibe-headless: no-gl renderer-unavailable reported\n");

    const sv_status loadStatus = sv_load_preset(ctx, presetPath.c_str(), 0.0f);
    printf("synesthesia-vibe-headless: sv_load_preset status=%d (%s) path=\"%s\"\n",
        static_cast<int>(loadStatus), BdStatusName(loadStatus), presetPath.c_str());

    if (loadStatus != SV_OK)
    {
        fprintf(stderr, "synesthesia-vibe-headless: FAIL unexpected preset load status, sv_last_error=\"%s\"\n",
            sv_last_error(ctx));
        sv_destroy(ctx);
        return EXIT_FAILURE;
    }

    const int framesRendered = BdRunFrames(ctx, outDir);
    sv_destroy(ctx);

    sv_context* badCtx = sv_create(&config);
    if (badCtx == nullptr)
    {
        fprintf(stderr, "synesthesia-vibe-headless: sv_create returned NULL for the bad-path run\n");
        return EXIT_FAILURE;
    }

    const std::string badPresetPath = repoRoot + "/" + kBadPresetRelativePath;
    const sv_status badStatus = sv_load_preset(badCtx, badPresetPath.c_str(), 0.0f);
    const char* badError = sv_last_error(badCtx);

    printf("synesthesia-vibe-headless: bad-path path=\"%s\"\n", badPresetPath.c_str());
    printf("synesthesia-vibe-headless: bad-path sv_load_preset status=%d (%s)\n",
        static_cast<int>(badStatus), BdStatusName(badStatus));
    printf("synesthesia-vibe-headless: bad-path sv_last_error=\"%s\"\n", badError);

    const bool badPathRejected = badStatus != SV_OK && badError != nullptr && badError[0] != '\0';
    sv_destroy(badCtx);

    if (!badPathRejected)
    {
        fprintf(stderr, "synesthesia-vibe-headless: FAIL bad-path load neither errored nor reported a message\n");
        return EXIT_FAILURE;
    }

    printf("synesthesia-vibe-headless: OK frames=%d frame_dir=\"%s\"\n", framesRendered, outDir.c_str());
    return EXIT_SUCCESS;
}