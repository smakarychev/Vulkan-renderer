#pragma once

#include <initializer_list>
#include <string>

namespace lux::assetlib::reflection
{
struct Dummy final {};
using MakeReflectable = std::initializer_list<Dummy>;


constexpr char asciiToUpper(char c) noexcept
{
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}
constexpr char asciiToLower(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 'a' - 'A') : c;
}

constexpr std::string toCamelCase(std::string_view sv)
{
    std::string out;
    out.reserve(sv.size());

    out.push_back(asciiToLower(sv[0]));
    bool upperNext = false;
    for (size_t i = 1; i < sv.size(); ++i) {
        const char c = sv[i];
        if (c == '_') {
            if (i + 1 < sv.size() && sv[i + 1] != '_') {
                upperNext = true;
            }
        }
        else {
            if (upperNext) {
                out.push_back(asciiToUpper(c));
                upperNext = false;
            }
            else {
                out.push_back(c);
            }
        }
    }
    return out;
}
struct CamelCase
{
    static constexpr std::string rename_key(const auto key) { return toCamelCase(key); }
};
}
