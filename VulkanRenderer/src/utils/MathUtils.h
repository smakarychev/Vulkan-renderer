#pragma once

#include <functional>
#include <intrin.h>

#include "types.h"

#ifndef _MSC_VER
#define clz32(x) __builtin_clz(x)
#else
#define clz32(x) __lzcnt(x)
#endif

namespace utils
{
    template <typename T>
    void hashCombine(u64& seed, const T& val)
    {
        std::hash<T> hasher;
        seed ^= hasher(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    inline u32 floorToPowerOf2(u32 number)
    {
        number |= number >> 1;
        number |= number >> 2;
        number |= number >> 4;
        number |= number >> 8;
        number |= number >> 16;
 
        return number ^ (number >> 1);
    }

    inline u32 log2(u32 number)
    {
        return 32 - clz32(number) - 1;
    }
}
