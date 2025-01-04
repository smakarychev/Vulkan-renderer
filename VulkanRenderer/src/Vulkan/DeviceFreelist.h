#pragma once

#include "types.h"

#include "Core/core.h"

#include "Rendering/ResourceHandle.h"

/* this struct should not be used as a general data structure,
 * it works only for pod objects or for
 * std::vector-like containers of pod-objects
 */

template <typename T>
class DeviceFreelist
{
    using OnResizeCallback = void (*)(T* oldMem, T* newMem);
    using OnSwapCallback = void (*)(T& a, T& b);
    
    static constexpr u32 NON_INDEX = std::numeric_limits<u32>::max();
    static_assert(sizeof(T) >= sizeof(u32), "Cannot use this type in the freelist");
public:
    using ValueType = T;

    ~DeviceFreelist();
    
    template <typename ... Args>
    constexpr ResourceHandleType<typename T::ObjectType> Add(Args&&... args);
    constexpr void Remove(ResourceHandleType<typename T::ObjectType> handle);

    // size of elements array, including the deleted ones
    constexpr u32 Capacity() const { return u32(m_DataCurrent - m_DataStart); }
    constexpr u32 Size() const { return m_Size; }
    
    constexpr const T& operator[](ResourceHandleType<typename T::ObjectType> handle) const;
    constexpr T& operator[](ResourceHandleType<typename T::ObjectType> handle);

    constexpr void SetOnResizeCallback(OnResizeCallback callback) { m_OnResizeCallback = callback; }
    constexpr void SetOnSwapCallback(OnSwapCallback) {}
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

    OnResizeCallback m_OnResizeCallback = [](T*, T*){};
};

template <typename T>
DeviceFreelist<T>::~DeviceFreelist()
{
    ASSERT(m_Size == 0, "Free list is not empty at the moment of destruction")
    delete[] (u8*)m_DataStart;
}

template <typename T>
constexpr const T& DeviceFreelist<T>::operator[](ResourceHandleType<typename T::ObjectType> handle) const
{
    ASSERT(handle.m_Id < Capacity(), "Element handle out of bounds")

    return m_DataStart[handle.m_Id];
}

template <typename T>
constexpr T& DeviceFreelist<T>::operator[](ResourceHandleType<typename T::ObjectType> handle)
{
    return const_cast<T&>(const_cast<const DeviceFreelist&>(*this)[handle.m_Id]);
}

template <typename T>
template <typename ... Args>
constexpr ResourceHandleType<typename T::ObjectType> DeviceFreelist<T>::Add(Args&&... args)
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
    
    return ResourceHandleType<typename T::ObjectType>{index};
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
    
    m_OnResizeCallback(m_DataStart, newData);
    
    delete[] (u8*)m_DataStart;
    m_DataStart = newData;
    m_DataCurrent = m_DataStart + oldSize;
    m_DataEnd = m_DataStart + newSize;
}

template <typename T>
constexpr void DeviceFreelist<T>::Remove(ResourceHandleType<typename T::ObjectType> handle)
{
    ASSERT(handle.m_Id < Capacity(), "Element handle out of bounds")

    m_DataStart[handle.m_Id].~T();
    *(u32*)((u8*)std::addressof(m_DataStart[handle.m_Id]) + (sizeof(T) - sizeof(u32))) = m_FirstFree;
    m_FirstFree = handle.m_Id;
    
    m_Size--;
}


