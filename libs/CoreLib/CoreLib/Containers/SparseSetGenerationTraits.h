#pragma once

#include <CoreLib/types.h>

#include <utility>

// todo: this needs allocator support

template <typename T>
struct SparseSetGenerationTraits
{
    static constexpr std::pair<u32, u32> Decompose(const T& val) { std::unreachable(); }
    static constexpr T Compose(u32 generation, u32 value) { std::unreachable(); }
};

template <>
struct SparseSetGenerationTraits<u32>
{
    static constexpr u32 GENERATION_BITS = 8;
    static constexpr u32 GENERATION_SHIFT = 32 - GENERATION_BITS;
    static constexpr u32 GENERATION_MASK = (u32)((1 << GENERATION_BITS) - 1) << GENERATION_SHIFT;
    static constexpr u32 INDEX_MASK = ~GENERATION_MASK;

    static constexpr std::pair<u32, u32> Decompose(const u32& val)
    {
        return std::make_pair((val & GENERATION_MASK) >> GENERATION_SHIFT, val & INDEX_MASK);
    }
    static constexpr u32 Compose(u32 generation, u32 value)
    {
        return ((generation << GENERATION_SHIFT) & GENERATION_MASK) | value;
    }
};