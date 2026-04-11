#pragma once

#include "Rendering/ResourceHandle.h"

#include <CoreLib/types.h>
#include <CoreLib/core.h>

/* this struct should not be used as a general data structure,
 * it works only for pod objects or for
 * std::vector-like containers of pod-objects
 */

template <typename T>
class DeviceFreelist
{
    static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();
    static_assert(sizeof(T) >= sizeof(u32), "Cannot use this type in the freelist");
    using Handle = ResourceHandleType<typename T::ObjectType>;
public:
    using ValueType = T;
    ~DeviceFreelist();
    
    template <typename ... Args>
    constexpr Handle Insert(Args&&... args);
    constexpr void Erase(Handle handle);

    // size of elements array, including the deleted ones
    constexpr u32 Capacity() const { return u32(m_DataCurrent - m_DataStart); }
    constexpr u32 Size() const { return m_Size; }
    
    constexpr const T& operator[](Handle handle) const;
    constexpr T& operator[](Handle handle);
private:
    template <typename ... Args>
    constexpr void EmplaceBack(Args&&... args);

    constexpr void Resize(u32 oldSize, u32 newSize);
private:
    T* m_DataStart{nullptr};    
    T* m_DataEnd{nullptr};    
    T* m_DataCurrent{nullptr};    
    
    u32 m_FirstFree{NON_INDEX};
    u32 m_Size{0};
};

template <typename T>
DeviceFreelist<T>::~DeviceFreelist()
{
    ASSERT(m_Size == 0, "Free list is not empty at the moment of destruction")
    delete[] (u8*)m_DataStart;
}

template <typename T>
constexpr const T& DeviceFreelist<T>::operator[](Handle handle) const
{
    ASSERT(handle.m_Id < Capacity(), "Element handle out of bounds")

    return m_DataStart[handle.m_Id];
}

template <typename T>
constexpr T& DeviceFreelist<T>::operator[](Handle handle)
{
    return const_cast<T&>(const_cast<const DeviceFreelist&>(*this)[handle.m_Id]);
}

template <typename T>
template <typename ... Args>
constexpr DeviceFreelist<T>::Handle DeviceFreelist<T>::Insert(Args&&... args)
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

    m_Size++;
    
    return Handle{index};
}

template <typename T>
template <typename ... Args>
constexpr void DeviceFreelist<T>::EmplaceBack(Args&&... args)
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
constexpr void DeviceFreelist<T>::Resize(u32 oldSize, u32 newSize)
{
    T* newData = (T*)new u8[newSize * sizeof(T)];
    // remember: if you silence an UB warning then it is UB no longer
    std::memcpy((void*)newData, (void*)m_DataStart, sizeof(T) * oldSize);
    
    delete[] (u8*)m_DataStart;
    m_DataStart = newData;
    m_DataCurrent = m_DataStart + oldSize;
    m_DataEnd = m_DataStart + newSize;
}

template <typename T>
constexpr void DeviceFreelist<T>::Erase(Handle handle)
{
    ASSERT(handle.m_Id < Capacity(), "Element handle out of bounds")

    m_DataStart[handle.m_Id].~T();
    *(u32*)((u8*)std::addressof(m_DataStart[handle.m_Id]) + (sizeof(T) - sizeof(u32))) = m_FirstFree;
    m_FirstFree = handle.m_Id;
    
    m_Size--;
}


