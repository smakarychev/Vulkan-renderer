#pragma once

#include <string_view>

#include "hash.h"

namespace Utils
{
    class StringHasher
    {
    public:
        // force string literals
        constexpr StringHasher(std::string_view string)
            : m_Hash(hashString(string)) {}

        StringHasher() = default;

        constexpr StringHasher(const StringHasher& other) = default;
        constexpr StringHasher(StringHasher&& other) noexcept = default;
        constexpr StringHasher& operator=(const StringHasher& other) = default;
        constexpr StringHasher& operator=(StringHasher&& other) noexcept = default;
    
        constexpr u64 GetHash() const { return m_Hash; }
    private:
        u64 m_Hash{};
    };

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
}

