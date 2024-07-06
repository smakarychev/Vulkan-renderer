#pragma once

#include "types.h"

#include <utility>

template <typename T>
struct SparseSetGenerationTraits
{
    static constexpr std::pair<u32, u32> Decompose(const T& val) { std::unreachable(); }
    static constexpr T Compose(u32 generation, u32 value) { std::unreachable(); }
};
