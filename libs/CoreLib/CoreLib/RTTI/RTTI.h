#pragma once

#include "TypeInfo.h"

namespace lux
{
struct RTTIHelper
{
    template <typename T, typename ... Parents>
    static TypeInfo GenerateTypeInfo(StringId typeName)
    {
        TypeInfo typeInfo = {.TypeName = typeName};
        typeInfo.Parents.reserve(sizeof...(Parents));
        (typeInfo.Parents.push_back(Parents::GetStaticTypeInfo()), ...);
        
        static_assert((std::is_base_of_v<Parents, T> && ...));
        
        return typeInfo;
    }
};

template <typename T>
concept HasIntrusiveRTTI = requires {
    typename T::IntrusiveRTTI;
};

template <HasIntrusiveRTTI T>
const TypeInfo& rttiStaticType() { return T::GetStaticTypeInfo(); }
template <HasIntrusiveRTTI T>
const TypeInfo& rttiType(const T* type) { return type->GetTypeInfo(); }
}

#define LUX_RTTI(type, ...) \
    using IntrusiveRTTI = void; \
    static const ::lux::TypeInfo& GetStaticTypeInfo() \
    { \
        static ::lux::TypeInfo typeInfo = ::lux::RTTIHelper::GenerateTypeInfo< \
            type, __VA_ARGS__>(SID(StringId::FromString(GENERATOR_PRETTY_FUNCTION))); \
        return typeInfo; \
    } \
    virtual const ::lux::TypeInfo& GetTypeInfo() const \
    { \
        return GetStaticTypeInfo(); \
    }

