#pragma once

#include "hash.h"

#include <string_view>

namespace utils
{
    class StringHash
    {
    public:
        StringHash(const StringHash&) = default;
        StringHash(StringHash&&) = default;
        StringHash& operator=(const StringHash&) = default;
        StringHash& operator=(StringHash&&) = default;
        ~StringHash() = default;
        
        StringHash(u64 hash)
            : m_Hash(hash) {}
        StringHash(std::string_view string)
            : m_Hash(utils::hashBytes(string.data(), (u32)string.size())) {}

        constexpr operator u64() const { return m_Hash; }
    private:
        u64 m_Hash{0};
    };
}
