#include "StringId.h"

#include <CoreLib/core.h>

#include <string>
#include <unordered_map>

namespace 
{
struct StringIdRegistryStorage
{
    std::unordered_map<u64, std::string> Strings;
    std::shared_mutex Mutex;
    
    StringIdRegistryStorage()
    {
        
        /* default constructable StringId has 0 hash value */
        Strings[0] = "NOT_A_STRING";
    }
};

StringIdRegistryStorage g_Storage = {};
}

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
    {
        std::shared_lock lock(g_Storage.Mutex);
        if (g_Storage.Strings.contains(m_Hash))
        {
            ASSERT(g_Storage.Strings.at(m_Hash) == string.String(), "Hash collision for StringId {} and {}",
                g_Storage.Strings.at(m_Hash), string.String())
            return;
        }
    }
    
    {
        std::scoped_lock lock(g_Storage.Mutex);
        g_Storage.Strings[m_Hash] = string.String();
    }
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
    std::shared_lock lock(g_Storage.Mutex);
    
    return g_Storage.Strings.at(m_Hash);
}

std::string_view StringId::AsStringView() const
{
    std::shared_lock lock(g_Storage.Mutex);
    
    return g_Storage.Strings.at(m_Hash);
}
