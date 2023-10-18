#pragma once

#include <limits>
#include <compare>

#include "types.h"

template <typename T>
class HandleArray;

template <typename T>
class RenderHandle
{
    friend class HandleArray<T>;
public:
    using UnderlyingType = u32;
    static constexpr UnderlyingType NON_HANDLE = std::numeric_limits<UnderlyingType>::max();
    RenderHandle()
        : m_Id(NON_HANDLE) {}
    RenderHandle(UnderlyingType id)
        : m_Id(id) {}
    RenderHandle(const RenderHandle& other) = default;
    RenderHandle(RenderHandle&& other) = default;
    RenderHandle& operator=(const RenderHandle& other) = default;
    RenderHandle& operator=(RenderHandle&& other) = default;
    ~RenderHandle() = default;

    friend auto operator<=>(const RenderHandle<T>& a, const RenderHandle<T>& b) = default;
private:
    UnderlyingType m_Id;
};
