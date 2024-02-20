#pragma once

#include "RenderingCommon.h"

template <typename T>
class ResourceHandle
{
    FRIEND_INTERNAL
public:
    friend auto operator<=>(const ResourceHandle& a, const ResourceHandle& b) = default;
    
    ResourceHandle() = default;
    ResourceHandle(const ResourceHandle&) = default;
    ResourceHandle(ResourceHandle&&) = default;
    ResourceHandle& operator=(const ResourceHandle&) = default;
    ResourceHandle& operator=(ResourceHandle&&) = default;
private:
    ResourceHandle(u32 index) : m_Index(index) {}
    
private:
    using UnderlyingType = u32;
    static constexpr UnderlyingType NON_HANDLE = std::numeric_limits<UnderlyingType>::max();
    UnderlyingType m_Index{NON_HANDLE};
};
