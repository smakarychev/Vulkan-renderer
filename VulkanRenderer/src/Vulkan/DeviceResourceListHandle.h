#pragma once

#include <CoreLib/types.h>

class Device;
class DeviceInternal;
class DeviceResources;
class DeletionQueue;

namespace lux
{
template <typename T>
class DeviceResourceListHandle
{
    friend class ::Device;
    friend class ::DeviceInternal;
    friend class ::DeviceResources;
    friend class ::DeletionQueue;
    template <typename Hot, typename Cold>
    friend class DeviceResourceList;
    
    static constexpr u32 INVALID = ~0u;
    static constexpr u32 GENERATION_SHIFT = 32u;
    static constexpr u64 INDEX_MASK = (1llu << GENERATION_SHIFT) - 1llu;
public:
    constexpr bool HasValue() const { return m_Index != INVALID; }
    auto operator<=>(const DeviceResourceListHandle&) const = default;
    
    constexpr DeviceResourceListHandle() = default;
    constexpr DeviceResourceListHandle(const DeviceResourceListHandle&) = default;
    constexpr DeviceResourceListHandle(DeviceResourceListHandle&&) = default;
    constexpr DeviceResourceListHandle& operator=(const DeviceResourceListHandle&) = default;
    constexpr DeviceResourceListHandle& operator=(DeviceResourceListHandle&&) = default;
private:
    constexpr u64 GetId() const { return (u64)m_Index | (u64)m_Generation << GENERATION_SHIFT; }
    DeviceResourceListHandle(u32 index, u32 generation) : m_Index(index), m_Generation(generation) {}
    DeviceResourceListHandle(u64 id) : m_Index(id & INDEX_MASK), m_Generation(id >> GENERATION_SHIFT) {}
private:
    u32 m_Index{INVALID};
    u32 m_Generation{};
};
}
