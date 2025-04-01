#pragma once

#include <unordered_map>

#include "types.h"
#include "HashedStringView.h"

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
    static StringId FromString(std::string_view string);

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
}