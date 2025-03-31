#pragma once

#include <unordered_map>

#include "types.h"

class HashedStringView;

class StringId
{
public:
    constexpr StringId() = default;
    StringId(const HashedStringView& string);
    /* It is implemented as static method because it is supposedly pretty easy to forget `sv`:
     *  StringId a = StringId("Hello"sv);
     *  StringId b = StringId("Hello");
     * Here, the `b` version does not have string hash at compile-time
     */
    static StringId FromString(const std::string& string);

    constexpr auto operator<=>(const StringId&) const = default;

    const std::string& AsString() const;
    std::string_view AsStringView() const;
private:
    u64 m_Hash{0};
};

class StringIdRegistry
{
    friend class StringId;
public:
    static void Init();
private:
    static std::unordered_map<u64, std::string> s_Strings;
};
