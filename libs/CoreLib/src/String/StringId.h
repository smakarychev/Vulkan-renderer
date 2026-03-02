#pragma once

#include <format>
#include <string>
#include <unordered_map>

#include "types.h"
#include "HashedStringView.h"

class StringId
{
public:
    constexpr StringId() = default;
    StringId(const HashedStringView& string);
    template <typename ... Types>
    requires (sizeof...(Types) > 0)
    StringId(const std::format_string<Types...>& formatString, Types&&... args);
    /* It is implemented as static method because it is supposedly pretty easy to forget `sv`:
     *  StringId a = StringId("Hello"sv);
     *  StringId b = StringId("Hello");
     * Here, the `b` version does not have string hash at compile-time
     */
    static StringId FromString(std::string_view string);

    StringId Concatenate(StringId other) const;
    StringId Concatenate(std::string_view other) const;
    StringId AddVersion(std::integral auto version) const;
    
    constexpr auto operator<=>(const StringId&) const = default;

    const std::string& AsString() const;
    std::string_view AsStringView() const;

    constexpr u64 Hash() const
    {
        return m_Hash;
    }
private:
    u64 m_Hash{0};
};

template <typename ... Types>
requires (sizeof...(Types) > 0)
StringId::StringId(const std::format_string<Types...>& formatString, Types&&... args)
{
    *this = FromString(std::format(formatString, std::forward<Types>(args)...));
}

StringId StringId::AddVersion(std::integral auto version) const
{
    return Concatenate("." + std::to_string(version));
}

class StringIdRegistry
{
    friend class StringId;
public:
    static void Init();
private:
    static std::unordered_map<u64, std::string> s_Strings;
};


namespace std
{
template <>
struct hash<StringId>
{
    usize operator()(const StringId stringId) const noexcept
    {
        return stringId.Hash();
    }
};

template <>
struct formatter<StringId> {
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    auto format(StringId id, format_context& ctx) const
    {
        return format_to(ctx.out(), "{}", id.AsStringView());
    }
};
}
