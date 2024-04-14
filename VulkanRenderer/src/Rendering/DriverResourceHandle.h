#pragma once

#include "RenderingCommon.h"

template <typename T>
class ResourceHandle
{
    FRIEND_INTERNAL
public:
    friend auto constexpr operator<=>(const ResourceHandle& a, const ResourceHandle& b) = default;

    constexpr bool HasValue() const { return m_Index != NON_HANDLE; }
    
    constexpr ResourceHandle() = default;
    constexpr ResourceHandle(const ResourceHandle&) = default;
    constexpr ResourceHandle(ResourceHandle&&) = default;
    constexpr ResourceHandle& operator=(const ResourceHandle&) = default;
    constexpr ResourceHandle& operator=(ResourceHandle&&) = default;
private:
    constexpr ResourceHandle(u32 index) : m_Index(index) {}
private:
    using UnderlyingType = u32;
    static constexpr UnderlyingType NON_HANDLE = std::numeric_limits<UnderlyingType>::max();
    UnderlyingType m_Index{NON_HANDLE};
};
