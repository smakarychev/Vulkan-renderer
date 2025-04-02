#include "StringId.h"

#include <string>
#include <unordered_map>

std::unordered_map<u64, std::string> StringIdRegistry::s_Strings;

static_assert(std::is_trivially_move_constructible_v<StringId>);
static_assert(std::is_trivially_move_assignable_v<StringId>);
static_assert(std::is_trivially_copy_constructible_v<StringId>);
static_assert(std::is_trivially_copy_assignable_v<StringId>);
static_assert(std::is_trivially_destructible_v<StringId>);
static_assert(std::is_trivially_copyable_v<StringId>);
static_assert(std::_Is_trivially_swappable_v<StringId>);

StringId::StringId(const HashedStringView& string)
    : m_Hash(string.Hash())
{
    if (StringIdRegistry::s_Strings.contains(m_Hash))
        return;
    StringIdRegistry::s_Strings[m_Hash] = string.String();
}

StringId StringId::FromString(std::string_view string)
{
    return {HashedStringView{string}};
}

StringId StringId::Concatenate(StringId other) const
{
    return FromString(AsString() + other.AsString());
}

StringId StringId::Concatenate(std::string_view other) const
{
    const std::string& string = AsString();
    std::string combined;
    combined.reserve(string.size() + other.size());
    combined.append_range(string).append_range(other);

    return FromString(combined);
}

const std::string& StringId::AsString() const
{
    return StringIdRegistry::s_Strings.at(m_Hash);
}

std::string_view StringId::AsStringView() const
{
    return StringIdRegistry::s_Strings.at(m_Hash);
}

void StringIdRegistry::Init()
{
    /* default constructable StringId has 0 hash value */
    s_Strings[0] = "NOT_A_STRING";
}
