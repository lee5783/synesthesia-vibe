//
//  rng_compat.cpp
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#include "rng_compat.h"

#include <cstdlib>

namespace
{
    thread_local uint32_t g_bdHoldrand = 1;

    void UMul128(uint64_t left, uint64_t right, uint64_t& lowResult, uint64_t& highResult)
    {
        const uint32_t leftLo = static_cast<uint32_t>(left);
        const uint32_t leftHi = static_cast<uint32_t>(left >> 32);
        const uint32_t rightLo = static_cast<uint32_t>(right);
        const uint32_t rightHi = static_cast<uint32_t>(right >> 32);

        uint64_t t = static_cast<uint64_t>(leftLo) * rightLo;
        const uint32_t w0 = static_cast<uint32_t>(t);
        uint64_t k = t >> 32;

        t = static_cast<uint64_t>(leftHi) * rightLo + k;
        const uint32_t w1 = static_cast<uint32_t>(t);
        const uint64_t w2 = t >> 32;

        t = static_cast<uint64_t>(leftLo) * rightHi + w1;
        k = t >> 32;

        lowResult = (t << 32) | w0;
        highResult = static_cast<uint64_t>(leftHi) * rightHi + w2 + k;
    }
}

int BdRandReimpl()
{
    // MSVC's CRT rand()/srand() run this exact LCG (per-thread state, default 1 until srand()
    // is called): holdrand = holdrand * 214013 + 2531011, result = (holdrand >> 16) & 0x7fff,
    // RAND_MAX == 32767. Documented CRT behavior, not a guess - this re-implementation exists so
    // non-MSVC hosts reproduce the same rand() stream as the MSVC-built DX9 golden captures.
    g_bdHoldrand = g_bdHoldrand * 214013u + 2531011u;
    return static_cast<int>((g_bdHoldrand >> 16) & 0x7fff);
}

void BdSrandReimpl(unsigned int seed)
{
    g_bdHoldrand = seed;
}

int BdRand()
{
#if defined(_MSC_VER)
    return rand();
#else
    return BdRandReimpl();
#endif
}

void BdSrand(unsigned int seed)
{
#if defined(_MSC_VER)
    srand(seed);
#else
    BdSrandReimpl(seed);
#endif
}

uint32_t BdUniformUint32FullRangeReimpl(std::mt19937_64& gen)
{
    return static_cast<uint32_t>(gen());
}

uint32_t BdUniformUint32Reimpl(std::mt19937_64& gen, uint32_t range)
{
    uint64_t low;
    uint64_t high;
    uint64_t x = gen();
    UMul128(x, static_cast<uint64_t>(range), low, high);

    if (low < range)
    {
        const uint64_t threshold = (0ULL - static_cast<uint64_t>(range)) % range;
        while (low < threshold)
        {
            x = gen();
            UMul128(x, static_cast<uint64_t>(range), low, high);
        }
    }

    return static_cast<uint32_t>(high);
}

float BdUniformCanonicalFloatReimpl(std::mt19937_64& gen)
{
    const uint64_t raw = gen();
    const uint32_t sx = static_cast<uint32_t>(raw >> 40);
    return static_cast<float>(sx) * (1.0f / 16777216.0f);
}
