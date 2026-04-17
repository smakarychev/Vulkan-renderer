#pragma once
#include <CoreLib/types.h>

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

namespace std
{
template <>
struct hash<lux::AssetHandleBase>
{
    usize operator()(const lux::AssetHandleBase handle) const noexcept
    {
        return std::hash<u64>{}((u64)handle.Index() | ((u64)handle.Version() << 32u));
    }
};
template <typename Resource>
struct hash<lux::AssetHandle<Resource>>
{
    usize operator()(const lux::AssetHandle<Resource> handle) const noexcept
    {
        return std::hash<lux::AssetHandleBase>{}(std::bit_cast<lux::AssetHandleBase>(handle));
    }
};
}
