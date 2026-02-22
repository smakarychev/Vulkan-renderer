#pragma once
#include "types.h"

namespace lux
{
class AssetHandleBase
{
    static constexpr u32 INVALID = ~0u;
public:
    constexpr bool IsValid() const
    {
        return m_Index != INVALID;
    }
    constexpr auto operator<=>(const AssetHandleBase&) const = default;

    constexpr AssetHandleBase() = default;
    constexpr AssetHandleBase(u32 index, u32 version) : m_Index(index), m_Version(version) {}

    constexpr u32 Index() const { return m_Index; }
    constexpr u32 Version() const { return m_Version; }
protected:
    u32 m_Index{INVALID};
    u32 m_Version{0};
};

template <typename Resource>
class AssetHandle : public AssetHandleBase
{
public:
    using AssetHandleBase::AssetHandleBase;
    constexpr AssetHandle() = default;
    constexpr AssetHandle(AssetHandleBase base) : AssetHandleBase(base) {}
    auto operator<=>(const AssetHandle&) const = default;
};
}
