//
//  rng_compat_test.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#include <gtest/gtest.h>

#include "rng_compat.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr unsigned int kSeed = 20260927u;
    constexpr int kDraws = 1000000;

    uint64_t Fnv1a64(const void* data, size_t len)
    {
        uint64_t hash = 14695981039346656037ULL;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i)
        {
            hash ^= bytes[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    template <typename T>
    uint64_t RunningHash(uint64_t hash, const T& value)
    {
        hash ^= Fnv1a64(&value, sizeof(value));
        hash *= 1099511628211ULL;
        return hash;
    }

    std::map<std::string, std::string> LoadReferenceFile()
    {
        std::map<std::string, std::string> values;
        std::string path = std::string(SYNESTHESIA_VIBE_RNG_TEST_DATA_DIR) + "/rng-reference-seed-20260927.txt";
        FILE* f = fopen(path.c_str(), "r");
        EXPECT_NE(f, nullptr) << "reference file not found at " << path;
        if (!f)
        {
            return values;
        }

        char key[128];
        char value[128];
        while (fscanf(f, "%127s %127s", key, value) == 2)
        {
            values[key] = value;
        }
        fclose(f);
        return values;
    }

    std::string HexHash(uint64_t hash)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(hash));
        return buf;
    }

    uint64_t HashRandReimplStream(unsigned int seed, int draws)
    {
        BdSrandReimpl(seed);
        uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < draws; ++i)
        {
            int v = BdRandReimpl();
            hash = RunningHash(hash, v);
        }
        return hash;
    }

    uint64_t HashRealRandStream(unsigned int seed, int draws)
    {
        srand(seed);
        uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < draws; ++i)
        {
            int v = rand();
            hash = RunningHash(hash, v);
        }
        return hash;
    }

    template <typename IntType>
    uint64_t HashUniformIntReimplStream(unsigned int seed, int draws, IntType lo, IntType hi)
    {
        std::mt19937_64 gen(seed);
        uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < draws; ++i)
        {
            IntType v = BdUniformIntReimpl<IntType>(gen, lo, hi);
            hash = RunningHash(hash, v);
        }
        return hash;
    }

    template <typename IntType>
    uint64_t HashUniformIntRealStream(unsigned int seed, int draws, IntType lo, IntType hi)
    {
        std::mt19937_64 gen(seed);
        std::uniform_int_distribution<IntType> dist(lo, hi);
        uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < draws; ++i)
        {
            IntType v = dist(gen);
            hash = RunningHash(hash, v);
        }
        return hash;
    }

    uint64_t HashUniformRealReimplStream(unsigned int seed, int draws, float lo, float hi)
    {
        std::mt19937_64 gen(seed);
        uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < draws; ++i)
        {
            float v = BdUniformRealReimpl<float>(gen, lo, hi);
            hash = RunningHash(hash, v);
        }
        return hash;
    }

    uint64_t HashUniformRealRealStream(unsigned int seed, int draws, float lo, float hi)
    {
        std::mt19937_64 gen(seed);
        std::uniform_real_distribution<float> dist(lo, hi);
        uint64_t hash = 14695981039346656037ULL;
        for (int i = 0; i < draws; ++i)
        {
            float v = dist(gen);
            hash = RunningHash(hash, v);
        }
        return hash;
    }
}

TEST(RngCompat, RandReimplMatchesReferenceHash)
{
    auto reference = LoadReferenceFile();
    ASSERT_EQ(reference["rand_lcg"], HexHash(HashRandReimplStream(kSeed, kDraws)));
}

#if defined(_MSC_VER)
TEST(RngCompat, RandReimplMatchesRealMsvcCrt)
{
    EXPECT_EQ(HashRandReimplStream(kSeed, kDraws), HashRealRandStream(kSeed, kDraws));
}

TEST(RngCompat, BdRandForwardsToRealCrtOnMsvc)
{
    BdSrand(kSeed);
    std::vector<int> bdValues;
    for (int i = 0; i < 1000; ++i)
    {
        bdValues.push_back(BdRand());
    }

    srand(kSeed);
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_EQ(bdValues[i], rand());
    }
}
#endif

struct UniformIntCase
{
    const char* key;
    int lo;
    int hi;
};

class RngCompatUniformInt : public ::testing::TestWithParam<UniformIntCase>
{
};

TEST_P(RngCompatUniformInt, ReimplMatchesReferenceHash)
{
    auto reference = LoadReferenceFile();
    const auto& c = GetParam();
    ASSERT_EQ(reference[c.key], HexHash(HashUniformIntReimplStream<int>(kSeed, kDraws, c.lo, c.hi)));
}

#if defined(_MSC_VER)
TEST_P(RngCompatUniformInt, ReimplMatchesRealMsvcStl)
{
    const auto& c = GetParam();
    EXPECT_EQ(HashUniformIntReimplStream<int>(kSeed, kDraws, c.lo, c.hi),
              HashUniformIntRealStream<int>(kSeed, kDraws, c.lo, c.hi));
}

TEST_P(RngCompatUniformInt, BdUniformIntDistributionForwardsToRealStlOnMsvc)
{
    const auto& c = GetParam();
    std::mt19937_64 genA(kSeed);
    std::mt19937_64 genB(kSeed);
    BdUniformIntDistribution<int> bdDist(c.lo, c.hi);
    std::uniform_int_distribution<int> stdDist(c.lo, c.hi);
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_EQ(bdDist(genA), stdDist(genB));
    }
}
#endif

INSTANTIATE_TEST_SUITE_P(ConsumerParameterSets, RngCompatUniformInt,
    ::testing::Values(
        UniformIntCase{"uniform_int_0_255", 0, 255},
        UniformIntCase{"uniform_int_0_215", 0, 215},
        UniformIntCase{"uniform_int_0_18", 0, 18},
        UniformIntCase{"uniform_int_0_64840", 0, 64840},
        UniformIntCase{"uniform_int_0_53750", 0, 53750},
        UniformIntCase{"uniform_int_0_42660", 0, 42660},
        UniformIntCase{"uniform_int_0_31570", 0, 31570},
        UniformIntCase{"uniform_int_0_63", 0, 63},
        UniformIntCase{"uniform_int_0_127", 0, 127}),
    [](const ::testing::TestParamInfo<UniformIntCase>& info) { return info.param.key; });

TEST(RngCompat, UniformIntSizeTZeroWidthReimplMatchesReferenceHash)
{
    auto reference = LoadReferenceFile();
    ASSERT_EQ(reference["uniform_int_size_t_0_0"],
              HexHash(HashUniformIntReimplStream<size_t>(kSeed, kDraws, size_t{0}, size_t{0})));
}

TEST(RngCompat, UniformIntSizeTReimplMatchesReferenceHash)
{
    auto reference = LoadReferenceFile();
    ASSERT_EQ(reference["uniform_int_size_t_0_41"],
              HexHash(HashUniformIntReimplStream<size_t>(kSeed, kDraws, size_t{0}, size_t{41})));
}

#if defined(_MSC_VER)
TEST(RngCompat, UniformIntSizeTReimplMatchesRealMsvcStl)
{
    EXPECT_EQ(HashUniformIntReimplStream<size_t>(kSeed, kDraws, size_t{0}, size_t{41}),
              HashUniformIntRealStream<size_t>(kSeed, kDraws, size_t{0}, size_t{41}));
}
#endif

TEST(RngCompat, UniformRealReimplMatchesReferenceHash)
{
    auto reference = LoadReferenceFile();
    ASSERT_EQ(reference["uniform_real_0_1"], HexHash(HashUniformRealReimplStream(kSeed, kDraws, 0.0f, 1.0f)));
}

#if defined(_MSC_VER)
TEST(RngCompat, UniformRealReimplMatchesRealMsvcStl)
{
    EXPECT_EQ(HashUniformRealReimplStream(kSeed, kDraws, 0.0f, 1.0f),
              HashUniformRealRealStream(kSeed, kDraws, 0.0f, 1.0f));
}

TEST(RngCompat, BdUniformRealDistributionForwardsToRealStlOnMsvc)
{
    std::mt19937_64 genA(kSeed);
    std::mt19937_64 genB(kSeed);
    BdUniformRealDistribution<float> bdDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> stdDist(0.0f, 1.0f);
    for (int i = 0; i < 1000; ++i)
    {
        ASSERT_EQ(bdDist(genA), stdDist(genB));
    }
}
#endif
