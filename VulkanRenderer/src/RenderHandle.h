#pragma once

#include "types.h"

#include <limits>
#include <compare>

template <typename T>
class RenderHandleArray;

template <typename T>
class RenderHandleDenseMap;

template <typename T>
class RenderHandle
{
    friend class RenderHandleArray<T>;
    friend class RenderHandleDenseMap<T>;
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

    friend auto operator<=>(const RenderHandle& a, const RenderHandle& b) = default;
private:
    UnderlyingType m_Id;
};
