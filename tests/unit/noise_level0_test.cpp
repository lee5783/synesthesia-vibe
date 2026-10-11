//
//  noise_level0_test.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-10-02.
//

#include <gtest/gtest.h>

#include "noise.h"
#include "time_model.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
    constexpr unsigned kDeterministicSeed = 20260927;
    constexpr const char* kCaptureId = "preset-cat-scan";

    class ZeroRawClock : public render_core::IRawClock
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

    struct NoiseTextureSpec
    {
        const char* name;
        int size;
        int zoomFactor;
        bool isVolume;
    };

    constexpr NoiseTextureSpec kNoiseSpecs[] = {
        {"noise_lq",      256, 1, false},
        {"noise_lq_lite",  32, 1, false},
        {"noise_mq",      256, 4, false},
        {"noise_hq",      256, 8, false},
        {"noisevol_lq",    32, 1, true},
        {"noisevol_hq",    32, 4, true},
    };

    std::string ResolvePhase2ReferenceDir()
    {
        if (const char* env = std::getenv("SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR"))
            return env;
        return SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR;
    }

    std::vector<uint8_t> ReadBinaryFile(const fs::path& path)
    {
        std::vector<uint8_t> data;
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return data;
        file.seekg(0, std::ios::end);
        std::streamoff size = file.tellg();
        if (size <= 0)
            return data;
        file.seekg(0);
        data.resize(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(data.data()), size);
        return data;
    }
}

TEST(NoiseLevel0Test, MatchesPhase2DumpNoiseReference)
{
    const std::string referenceDir = ResolvePhase2ReferenceDir();
    const fs::path dumpsDir = fs::path(referenceDir) / kCaptureId / "dumps";
    if (!fs::exists(dumpsDir))
    {
        GTEST_SKIP() << "P2-017 --dump-noise reference data not found at " << dumpsDir
                     << " (gitignored, local-only; set SYNESTHESIA_VIBE_PHASE2_REFERENCE_DIR to point at tests/golden/phase2-reference "
                        "or an archive re-extraction of it; noise is byte-identical across all 13 P2-017 captures, so any one works)";
    }

    int texturesChecked = 0;
    for (const NoiseTextureSpec& spec : kNoiseSpecs)
    {
        // The real DX9 host seeds `gen` with the deterministic-mode seed and generates all 6 level-0
        // noise textures back-to-back, right after PluginInitialize, before the first AdvanceFrame()
        // call (apps/beatdrop-demo-windows/determinism.cpp) - so the fake QueryPerformanceCounter it
        // patches in always reports tick 0 at this point. ZeroRawClock reproduces that exactly, and
        // `gen` must be freshly re-seeded per texture here since the real run never resets it between
        // AddNoiseTex/AddNoiseVol calls (each texture's reseed rows consume further off the one
        // running generator) - so this test advances one shared `gen` across the whole kNoiseSpecs
        // loop, in the same order plugin.cpp calls AddNoiseTex/AddNoiseVol in AllocateMyDX9Stuff.
        static std::mt19937_64 gen(kDeterministicSeed);
        static ZeroRawClock clock;

        std::vector<uint32_t> generated(static_cast<size_t>(spec.size) * spec.size * (spec.isVolume ? spec.size : 1));
        if (spec.isVolume)
            render_core::GenerateNoiseVol(generated.data(), spec.size, spec.zoomFactor, gen, clock);
        else
            render_core::GenerateNoiseTex(generated.data(), spec.size, spec.zoomFactor, gen, clock);

        const fs::path binPath = dumpsDir / (std::string(spec.name) + ".bgra");
        std::vector<uint8_t> reference = ReadBinaryFile(binPath);
        ASSERT_EQ(reference.size(), generated.size() * sizeof(uint32_t))
            << spec.name << " reference size mismatch at " << binPath;

        const uint32_t* refPixels = reinterpret_cast<const uint32_t*>(reference.data());
        for (size_t i = 0; i < generated.size(); ++i)
        {
            EXPECT_EQ(refPixels[i], generated[i]) << spec.name << " pixel " << i;
        }

        ++texturesChecked;
    }

    EXPECT_EQ(texturesChecked, static_cast<int>(sizeof(kNoiseSpecs) / sizeof(kNoiseSpecs[0])));
}
