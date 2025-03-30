#pragma once

#include "Utils/Hash.h"

/* NOTE: this object does not own the string */
class HashedStringView
{
public:
    constexpr HashedStringView(const std::string_view string)
        : m_Hash(Hash::string(string)), m_String(string) {}

    constexpr HashedStringView() = default;
    constexpr ~HashedStringView() = default;

    constexpr HashedStringView(const HashedStringView& other) = default;
    constexpr HashedStringView(HashedStringView&& other) noexcept = default;
    constexpr HashedStringView& operator=(const HashedStringView& other) = default;
    constexpr HashedStringView& operator=(HashedStringView&& other) noexcept = default;

    constexpr u64 Hash() const { return m_Hash; }
    constexpr std::string_view String() const { return m_String; }
private:
    u64 m_Hash{};
    std::string_view m_String;
};

