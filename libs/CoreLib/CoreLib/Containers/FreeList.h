#pragma once
#include <CoreLib/core.h>
#include <CoreLib/types.h>
#include <CoreLib/Containers/PagedDenseArray.h>

namespace lux
{
// todo: this needs allocator support

template <typename T>
class FreeList
{
    static constexpr u32 NO_FREE = ~0u;
    using FreeIndexType = u32;
    static_assert(sizeof(T) >= sizeof(FreeIndexType), "Cannot use this type in the freelist");
    static_assert(std::is_trivially_destructible_v<T>, "Type must be trivially destructible");
public:
    template <typename ... Args>
    constexpr u32 Insert(Args&&... args);
    template <typename ... Args>
    constexpr u32 insert(Args&&... args) { return Insert(std::forward<Args>(args)...); }
    constexpr void Erase(u32 index);
    constexpr void erase(u32 index) { Erase(index); }

    // size of elements array, including the deleted ones
    constexpr u32 Size() const { return m_Size; }
    constexpr u32 size() const { return Size(); }
    
    constexpr const T& operator[](u32 index) const;
    constexpr T& operator[](u32 index);
private:
    PagedDenseArray<T> m_Elements;
    
    u32 m_FirstFree{NO_FREE};
    u32 m_Size{0};
};

template <typename T>
template <typename ... Args>
constexpr u32 FreeList<T>::Insert(Args&&... args)
{
    u32 index;  
    if (m_FirstFree != NO_FREE)
    {
        index = m_FirstFree;
        m_FirstFree = *(const u32*)std::addressof(m_Elements[index]);
        std::construct_at(std::addressof(m_Elements[index]), std::forward<Args>(args)...);
    }
    else
    {
        index = m_Elements.insert(std::forward<Args>(args)...);
    }

    m_Size++;
    
    return index;
}

template <typename T>
constexpr void FreeList<T>::Erase(u32 index)
{
    ASSERT(index < m_Elements.capacity(), "Element handle out of bounds")

    *(u32*)std::addressof(m_Elements[index]) = m_FirstFree;
    m_FirstFree = index;
    
    m_Size--;
}

template <typename T>
constexpr const T& FreeList<T>::operator[](u32 index) const
{
    ASSERT(index < m_Elements.capacity(), "Element handle out of bounds")

    return m_Elements[index];
}

template <typename T>
constexpr T& FreeList<T>::operator[](u32 index)
{
    return const_cast<T&>(const_cast<const FreeList&>(*this)[index]);
}
}
