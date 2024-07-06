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
}

