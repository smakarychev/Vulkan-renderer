#pragma once

#include "types.h"

#include <vector>

template <typename T>
class Freelist
{
    static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();
    static_assert(sizeof(T) >= sizeof(u32), "Cannot use this type in the freelist");
public:
    using ValueType = T;

    ~Freelist();
    
    template <typename ... Args>
    constexpr u32 Add(Args&&... args);
    constexpr void Remove(u32 index);

    // size of elements array, including the deleted ones
    constexpr u32 Capacity() const { return m_Elements.size(); }
    constexpr u32 Count() const { return m_Count; }
    
    constexpr void Clear();

    constexpr const T& operator[](u32 index) const;
    constexpr T& operator[](u32 index);
    
private:
    std::vector<T> m_Elements;
    u32 m_FirstFree{NON_INDEX};
    u32 m_Count{0};
};

template <typename T>
Freelist<T>::~Freelist()
{
    Clear();
}

template <typename T>
constexpr void Freelist<T>::Clear()
{
    m_Elements.clear();
    m_Count = 0;
    m_FirstFree = NON_INDEX;
}

template <typename T>
constexpr const T& Freelist<T>::operator[](u32 index) const
{
    ASSERT(index < m_Elements.size(), "Element index out of bounds")

    return m_Elements[index];
}

template <typename T>
constexpr T& Freelist<T>::operator[](u32 index)
{
    return const_cast<T&>(const_cast<const Freelist&>(*this)[index]);
}

template <typename T>
template <typename ... Args>
constexpr u32 Freelist<T>::Add(Args&&... args)
{
    u32 index;
    if (m_FirstFree != NON_INDEX)
    {
        index = m_FirstFree;
        // write to the back of the element, otherwise we might override sensible header,
        // which will crash on destructor. Note that this whole thing is still an UB 
        m_FirstFree = *(const u32*)((u8*)std::addressof(m_Elements[index]) + (sizeof(T) - sizeof(u32)));
        new (std::addressof(m_Elements[index])) T(std::forward<Args>(args)...);
    }
    else
    {
        index = (u32)m_Elements.size();
        m_Elements.emplace_back(std::forward<Args>(args)...);     
    }

    m_Count++;
    
    return index;
}

template <typename T>
constexpr void Freelist<T>::Remove(u32 index)
{
    ASSERT(index < m_Elements.size(), "Element index out of bounds")

    m_Elements[index].~T();
    *(u32*)((u8*)std::addressof(m_Elements[index]) + (sizeof(T) - sizeof(u32))) = m_FirstFree;
    m_FirstFree = index;
    
    m_Count--;
}


