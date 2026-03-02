#pragma once


#include "core.h"
#include "types.h"

#include <functional>
#include <intrin.h>

#ifndef _MSC_VER
#define clz32(x) __builtin_clz(x)
#else
#define clz32(x) __lzcnt(x)
#endif

namespace Math
{
    constexpr u32 floorToPowerOf2(u32 number)
    {
        number |= number >> 1;
        number |= number >> 2;
        number |= number >> 4;
        number |= number >> 8;
        number |= number >> 16;
 
        return number ^ (number >> 1);
    }

    constexpr bool isPowerOf2(std::integral auto number)
    {
        return (number & (number - 1)) == 0;
    }

    inline u32 log2(u32 number)
    {
        return 32 - clz32(number) - 1;
    }

    constexpr std::floating_point auto lerp(std::floating_point auto a, std::floating_point auto b,
        std::floating_point auto t)
    {
        return a + (b - a) * t;
    }

    constexpr std::floating_point auto ilerp(std::floating_point auto a, std::floating_point auto b,
        std::floating_point auto t)
    {
        return (t - a) / (b - a);
    }

    /* Returns `value mod base` when base is the power of 2 */
    constexpr std::integral auto fastMod(std::integral auto value, std::integral auto base)
    {
        ASSERT(isPowerOf2(base), "Base have to be a power of 2")
        return value & (base - 1);
    }

}