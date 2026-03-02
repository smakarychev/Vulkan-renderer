#pragma once

#include "types.h"

#include <unordered_map>
#include <unordered_set>

struct StringHeterogeneousHasher
{
    using HashType = std::hash<std::string_view>;
    using is_transparent = void;

    u64 operator()(const char* str) const { return HashType{}(str); }
    u64 operator()(std::string_view str) const { return HashType{}(str); }
    u64 operator()(const std::string& str) const { return HashType{}(str); }
};

template <typename T>
using StringUnorderedMap = std::unordered_map<std::string, T, StringHeterogeneousHasher, std::equal_to<>>;
using StringUnorderedSet = std::unordered_set<std::string, StringHeterogeneousHasher, std::equal_to<>>;