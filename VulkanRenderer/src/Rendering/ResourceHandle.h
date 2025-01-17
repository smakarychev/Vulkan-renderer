#pragma once

#include "RenderingCommon.h"
#include "Common/SparseSetGenerationTraits.h"

template <typename T>
class DeviceFreelist;

template <typename T>
class ResourceHandle
{
    FRIEND_INTERNAL
    template <typename U>
    friend class DeviceFreelist;
public:
    friend auto constexpr operator<=>(const ResourceHandle& a, const ResourceHandle& b) = default;

    constexpr bool HasValue() const { return m_Id != NON_HANDLE; }
    
    constexpr ResourceHandle() = default;
    constexpr ResourceHandle(const ResourceHandle&) = default;
    constexpr ResourceHandle(ResourceHandle&&) = default;
    constexpr ResourceHandle& operator=(const ResourceHandle&) = default;
    constexpr ResourceHandle& operator=(ResourceHandle&&) = default;
private:
    constexpr ResourceHandle(u32 index) : m_Id(index) {}
private:
    using UnderlyingType = u32;
    static constexpr UnderlyingType NON_HANDLE = std::numeric_limits<UnderlyingType>::max();
    UnderlyingType m_Id{NON_HANDLE};
};

template <typename T>
class GenerationalResourceHandle;

template <typename T>
struct SparseSetGenerationTraits<GenerationalResourceHandle<T>>
{
    using Handle = GenerationalResourceHandle<T>;
    static constexpr u32 GENERATION_BITS = 8;
    static constexpr u32 GENERATION_SHIFT = 32 - GENERATION_BITS;
    static constexpr u32 GENERATION_MASK = (u32)((1 << GENERATION_BITS) - 1) << GENERATION_SHIFT;
    static constexpr u32 INDEX_MASK = ~GENERATION_MASK;
    
    static constexpr std::pair<u32, u32> Decompose(const Handle& val);
    static constexpr Handle Compose(u32 generation, u32 value);
};

template <typename T>
class GenerationalResourceHandle
{
    FRIEND_INTERNAL
    friend struct SparseSetGenerationTraits<GenerationalResourceHandle>;
    using Traits = SparseSetGenerationTraits<GenerationalResourceHandle>;
public:
    friend auto constexpr operator<=>(const GenerationalResourceHandle& a,
        const GenerationalResourceHandle& b) = default;

    constexpr bool HasValue() const { return m_Id != NON_HANDLE; }

    constexpr GenerationalResourceHandle() = default;
    constexpr GenerationalResourceHandle(const GenerationalResourceHandle&) = default;
    constexpr GenerationalResourceHandle(GenerationalResourceHandle&&) = default;
    constexpr GenerationalResourceHandle& operator=(const GenerationalResourceHandle&) = default;
    constexpr GenerationalResourceHandle& operator=(GenerationalResourceHandle&&) = default;
private:
    constexpr GenerationalResourceHandle(u32 id) : m_Id(id) {}
private:
    using UnderlyingType = u32;
    static constexpr UnderlyingType NON_HANDLE = std::numeric_limits<UnderlyingType>::max();
    UnderlyingType m_Id{NON_HANDLE};
};

template <typename T>
constexpr std::pair<u32, u32> SparseSetGenerationTraits<GenerationalResourceHandle<T>>::Decompose(
    const GenerationalResourceHandle<T>& val)
{
    return std::make_pair(
        (val.m_Id & GENERATION_MASK) >> GENERATION_SHIFT,
         val.m_Id & INDEX_MASK);
}

template <typename T>
constexpr GenerationalResourceHandle<T> SparseSetGenerationTraits<GenerationalResourceHandle<T>>::Compose(
    u32 generation, u32 value)
{
    return Handle(((generation << GENERATION_SHIFT) & GENERATION_MASK) | value);
}

/* this can be either GenerationalResourceHandle or just ResourceHandle */
template <typename T>
using ResourceHandleType = GenerationalResourceHandle<T>;