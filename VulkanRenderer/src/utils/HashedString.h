#pragma once

#include <string_view>
#include <unordered_map>

#include "hash.h"

namespace Utils
{
    /* NOTE: this object does not own the string */
    class HashedString
    {
    public:
        constexpr HashedString(const std::string_view string)
            : m_Hash(hashString(string)), m_String(string) {}

        constexpr HashedString() = default;
        constexpr ~HashedString() = default;

        constexpr HashedString(const HashedString& other) = default;
        constexpr HashedString(HashedString&& other) noexcept = default;
        constexpr HashedString& operator=(const HashedString& other) = default;
        constexpr HashedString& operator=(HashedString&& other) noexcept = default;
    
        constexpr u64 Hash() const { return m_Hash; }
        constexpr std::string_view String() const { return m_String; }
    private:
        u64 m_Hash{};
        std::string_view m_String;
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

