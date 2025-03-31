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

StringId StringId::FromString(const std::string& string)
{
    return {HashedStringView{string}};
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
