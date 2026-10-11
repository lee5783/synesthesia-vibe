//
//  rng_compat.h
//  SynesthesiaVibe
//
//  Created by lee5783 on 2026-09-30.
//

#pragma once

#include <cstdint>
#include <random>
#include <type_traits>

int BdRand();
void BdSrand(unsigned int seed);

int BdRandReimpl();
void BdSrandReimpl(unsigned int seed);

uint32_t BdUniformUint32Reimpl(std::mt19937_64& gen, uint32_t range);
uint32_t BdUniformUint32FullRangeReimpl(std::mt19937_64& gen);
float BdUniformCanonicalFloatReimpl(std::mt19937_64& gen);

template <typename IntType>
IntType BdUniformIntReimpl(std::mt19937_64& gen, IntType lo, IntType hi)
{
    using UInt = typename std::make_unsigned<IntType>::type;
    constexpr bool kIsSigned = std::is_signed<IntType>::value;
    constexpr UInt kAdjuster = kIsSigned ? static_cast<UInt>((static_cast<UInt>(~UInt{0}) >> 1) + 1) : UInt{0};

    const UInt umin = kIsSigned ? static_cast<UInt>(static_cast<UInt>(lo) ^ kAdjuster) : static_cast<UInt>(lo);
    const UInt umax = kIsSigned ? static_cast<UInt>(static_cast<UInt>(hi) ^ kAdjuster) : static_cast<UInt>(hi);

    UInt uret;
    if (static_cast<UInt>(umax - umin) == static_cast<UInt>(~UInt{0}))
    {
        uret = static_cast<UInt>(BdUniformUint32FullRangeReimpl(gen));
    }
    else
    {
        uret = static_cast<UInt>(BdUniformUint32Reimpl(gen, static_cast<uint32_t>(umax - umin + 1)));
    }

    UInt result = static_cast<UInt>(uret + umin);
    if (kIsSigned)
    {
        result = static_cast<UInt>(result ^ kAdjuster);
    }
    return static_cast<IntType>(result);
}

template <typename RealType>
RealType BdUniformRealReimpl(std::mt19937_64& gen, RealType lo, RealType hi)
{
    return static_cast<RealType>(BdUniformCanonicalFloatReimpl(gen)) * (hi - lo) + lo;
}

template <typename IntType>
class BdUniformIntDistribution
{
public:
    BdUniformIntDistribution(IntType lo, IntType hi) : lo_(lo), hi_(hi) {}

    IntType operator()(std::mt19937_64& gen) const
    {
#if defined(_MSC_VER)
        std::uniform_int_distribution<IntType> d(lo_, hi_);
        return d(gen);
#else
        return BdUniformIntReimpl<IntType>(gen, lo_, hi_);
#endif
    }

private:
    IntType lo_;
    IntType hi_;
};

template <typename RealType>
class BdUniformRealDistribution
{
public:
    BdUniformRealDistribution(RealType lo, RealType hi) : lo_(lo), hi_(hi) {}

    RealType operator()(std::mt19937_64& gen) const
    {
#if defined(_MSC_VER)
        std::uniform_real_distribution<RealType> d(lo_, hi_);
        return d(gen);
#else
        return BdUniformRealReimpl<RealType>(gen, lo_, hi_);
#endif
    }

private:
    RealType lo_;
    RealType hi_;
};
