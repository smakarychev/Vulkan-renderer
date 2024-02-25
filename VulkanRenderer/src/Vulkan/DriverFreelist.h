#pragma once

#include "types.h"

#include <vector>

#include "Core/core.h"

// this struct should not be used as a general data structure,
// it works only for pod objects or for
// std::vector-like containers of pod-objects

template <typename T>
class DriverFreelist
{
    using OnResizeCallback = void (*)(T* oldMem, T* newMem);
    
    static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();
    static_assert(sizeof(T) >= sizeof(u32), "Cannot use this type in the freelist");
public:
    using ValueType = T;

    ~DriverFreelist();
    
    template <typename ... Args>
    constexpr u32 Add(Args&&... args);
    constexpr void Remove(u32 index);

    // size of elements array, including the deleted ones
    constexpr u32 Capacity() const { return u32(m_DataCurrent - m_DataStart); }
    constexpr u32 Count() const { return m_Count; }
    
    constexpr const T& operator[](u32 index) const;
    constexpr T& operator[](u32 index);

    constexpr void SetOnResizeCallback(OnResizeCallback callback) { m_OnResizeCallback = callback; }
private:
    template <typename ... Args>
    constexpr void EmplaceBack(Args&&... args);

    constexpr void Resize(u32 oldSize, u32 newSize);
private:
    T* m_DataStart{nullptr};    
    T* m_DataEnd{nullptr};    
    T* m_DataCurrent{nullptr};    
    
    u32 m_FirstFree{NON_INDEX};
    u32 m_Count{0};

    OnResizeCallback m_OnResizeCallback = [](T*, T*){};
    Driver* m_Driver{nullptr};
};

template <typename T>
DriverFreelist<T>::~DriverFreelist()
{
    ASSERT(m_Count == 0, "Free list is not empty at the moment of destruction")
    delete[] (u8*)m_DataStart;
}

template <typename T>
constexpr const T& DriverFreelist<T>::operator[](u32 index) const
{
    ASSERT(index < Capacity(), "Element index out of bounds")

    return m_DataStart[index];
}

template <typename T>
constexpr T& DriverFreelist<T>::operator[](u32 index)
{
    return const_cast<T&>(const_cast<const DriverFreelist&>(*this)[index]);
}

template <typename T>
template <typename ... Args>
constexpr u32 DriverFreelist<T>::Add(Args&&... args)
{
    u32 index;  
    if (m_FirstFree != NON_INDEX)
    {
        index = m_FirstFree;
        // write to the back of the element, otherwise we might override sensible header,
        // which will crash on destructor. Note that this whole thing is still an UB 
        m_FirstFree = *(const u32*)((u8*)std::addressof(m_DataStart[index]) + (sizeof(T) - sizeof(u32)));
        std::construct_at(std::addressof(m_DataStart[index]), std::forward<Args>(args)...);
    }
    else
    {
        index = Capacity();
        EmplaceBack(std::forward<Args>(args)...);
    }

    m_Count++;
    
    return index;
}

template <typename T>
template <typename ... Args>
constexpr void DriverFreelist<T>::EmplaceBack(Args&&... args)
{
    if (m_DataCurrent == m_DataEnd)
    {
        u32 size = (u32)(m_DataEnd - m_DataStart);
        Resize(size, (u32)((f32)(size + 1) * 1.5f));
    }
    std::construct_at(m_DataCurrent, std::forward<Args>(args)...);
    m_DataCurrent++;
}

template <typename T>
constexpr void DriverFreelist<T>::Resize(u32 oldSize, u32 newSize)
{
    T* newData = (T*)new u8[newSize * sizeof(T)];
    // remember: if you silence an UB warning then it is UB no longer
    std::memcpy((void*)newData, (void*)m_DataStart, sizeof(T) * oldSize);
    
    m_OnResizeCallback(m_DataStart, newData);
    
    delete[] (u8*)m_DataStart;
    m_DataStart = newData;
    m_DataCurrent = m_DataStart + oldSize;
    m_DataEnd = m_DataStart + newSize;
}

template <typename T>
constexpr void DriverFreelist<T>::Remove(u32 index)
{
    ASSERT(index < Capacity(), "Element index out of bounds")

    m_DataStart[index].~T();
    *(u32*)((u8*)std::addressof(m_DataStart[index]) + (sizeof(T) - sizeof(u32))) = m_FirstFree;
    m_FirstFree = index;
    
    m_Count--;
}


