#pragma once

#include <CoreLib/Containers/Guid.h>
#include <CoreLib/String/StringId.h>

#include <algorithm>

namespace lux
{
struct TypeInfo
{
    StringId TypeName{};
    std::vector<TypeInfo> Parents{};

    constexpr bool operator==(const TypeInfo& other) const
    {
        return TypeName == other.TypeName;
    }
    
    constexpr bool IsExactlyA(const TypeInfo& type) const
    {
        return TypeName == type.TypeName;
    }

    template <typename T>
    constexpr bool IsExactlyA() const
    {
        return IsExactlyA(T::GetStaticTypeInfo());
    }
    
    bool IsA(const TypeInfo& type) const
    {
        if (IsExactlyA(type))
            return true;

        return std::ranges::any_of(Parents, [&type](const auto& parent){ return parent.IsA(type); });
    }

    template <typename T>
    bool IsA() const
    {
        return IsA(T::GetStaticTypeInfo());
    }
};
}

namespace std
{
template <>
struct hash<lux::TypeInfo>
{
    usize operator()(const lux::TypeInfo& typeInfo) const noexcept
    {
        return std::hash<StringId>{}(typeInfo.TypeName);
    }
};
}