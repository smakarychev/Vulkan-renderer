#include "StringId.h"

#include <CoreLib/core.h>

#include <string>
#include <unordered_map>

namespace 
{
// todo: there will be problems if I link CoreLib (a static lib) into multiple .dll
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

StringIdRegistryStorage& getStorage()
{
    static StringIdRegistryStorage storage = {};
    
    return storage;
}
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
        std::shared_lock lock(getStorage().Mutex);
        if (getStorage().Strings.contains(m_Hash))
        {
            ASSERT(getStorage().Strings.at(m_Hash) == string.String(), "Hash collision for StringId {} and {}",
                getStorage().Strings.at(m_Hash), string.String())
            return;
        }
    }
    
    {
        std::scoped_lock lock(getStorage().Mutex);
        getStorage().Strings[m_Hash] = string.String();
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
    std::shared_lock lock(getStorage().Mutex);
    
    return getStorage().Strings.at(m_Hash);
}

std::string_view StringId::AsStringView() const
{
    std::shared_lock lock(getStorage().Mutex);
    
    return getStorage().Strings.at(m_Hash);
}
