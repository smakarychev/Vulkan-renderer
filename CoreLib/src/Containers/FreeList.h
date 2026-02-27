#pragma once
#include "core.h"
#include "types.h"

namespace lux
{
template <typename T>
class FreeList
{
    static constexpr u32 NO_FREE = ~0u;
    using FreeIndexType = u32;
    static_assert(sizeof(T) >= sizeof(FreeIndexType), "Cannot use this type in the freelist");
    static_assert(std::is_trivially_destructible_v<T>, "Type must be trivially destructible");
public:
    ~FreeList();
    
    template <typename ... Args>
    constexpr u32 Insert(Args&&... args);
    template <typename ... Args>
    constexpr u32 insert(Args&&... args) { return Insert(std::forward<Args>(args)...); }
    constexpr void Erase(u32 index);
    constexpr void erase(u32 index) { Erase(index); }

    // size of elements array, including the deleted ones
    constexpr u32 Capacity() const { return u32(m_DataCurrent - m_DataStart); }
    constexpr u32 capacity() const { return Capacity(); }
    constexpr u32 Size() const { return m_Size; }
    constexpr u32 size() const { return Size(); }
    
    constexpr const T& operator[](u32 index) const;
    constexpr T& operator[](u32 index);
private:
    template <typename ... Args>
    constexpr void EmplaceBack(Args&&... args);

    constexpr void Resize(u32 oldSize, u32 newSize);
private:
    T* m_DataStart{nullptr};    
    T* m_DataEnd{nullptr};    
    T* m_DataCurrent{nullptr};    
    
    u32 m_FirstFree{NO_FREE};
    u32 m_Size{0};
};

template <typename T>
FreeList<T>::~FreeList()
{
    // type is is_trivially_destructible_v, so this should not leak
    delete[] (u8*)m_DataStart;
}

template <typename T>
template <typename ... Args>
constexpr u32 FreeList<T>::Insert(Args&&... args)
{
    u32 index;  
    if (m_FirstFree != NO_FREE)
    {
        index = m_FirstFree;
        m_FirstFree = *(const u32*)std::addressof(m_DataStart[index]);
        std::construct_at(std::addressof(m_DataStart[index]), std::forward<Args>(args)...);
    }
    else
    {
        index = Capacity();
        EmplaceBack(std::forward<Args>(args)...);
    }

    m_Size++;
    
    return index;
}

template <typename T>
constexpr void FreeList<T>::Erase(u32 index)
{
    ASSERT(index < Capacity(), "Element handle out of bounds")

    *(u32*)std::addressof(m_DataStart[index]) = m_FirstFree;
    m_FirstFree = index;
    
    m_Size--;
}

template <typename T>
constexpr const T& FreeList<T>::operator[](u32 index) const
{
    ASSERT(index < Capacity(), "Element handle out of bounds")

    return m_DataStart[index];
}

template <typename T>
constexpr T& FreeList<T>::operator[](u32 index)
{
    return const_cast<T&>(const_cast<const FreeList&>(*this)[index]);
}

template <typename T>
template <typename ... Args>
constexpr void FreeList<T>::EmplaceBack(Args&&... args)
{
    if (m_DataCurrent == m_DataEnd)
    {
        const u32 size = (u32)(m_DataEnd - m_DataStart);
        Resize(size, (u32)((f32)(size + 1) * 1.5f));
    }
    std::construct_at(m_DataCurrent, std::forward<Args>(args)...);
    m_DataCurrent++;
}

template <typename T>
constexpr void FreeList<T>::Resize(u32 oldSize, u32 newSize)
{
    T* newData = (T*)new u8[newSize * sizeof(T)];
    std::memcpy((void*)newData, (void*)m_DataStart, sizeof(T) * oldSize);
    
    delete[] (u8*)m_DataStart;
    m_DataStart = newData;
    m_DataCurrent = m_DataStart + oldSize;
    m_DataEnd = m_DataStart + newSize;
}
}
