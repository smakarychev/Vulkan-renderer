#pragma once

#include "Rendering/ResourceHandle.h"

#include <CoreLib/Containers/SlotMapType.h>

template <typename T>
class DeviceSparseSet
{
    using Handle = GenerationalResourceHandle<typename T::ObjectType>;
    using HandleSparseSet = lux::SparseSetType<u32, Handle>;
    using Traits = lux::SparseSetGenerationTraits<Handle>;
    using ResourceSet = lux::PagedDenseArray<T>;
public:
    using ValueType = T;
    
    template <typename ... Args>
    constexpr Handle Insert(Args&&... args);
    constexpr void Erase(Handle handle);

    constexpr const T& operator[](Handle handle) const;
    constexpr T& operator[](Handle handle);
    
    constexpr u32 Count() const { return (u32)m_SlotMap.Size(); }
    constexpr u32 Capacity() const { return (u32)m_SlotMap.Capacity(); }

    constexpr void Clear();
private:
    lux::SlotMapType<T, Handle> m_SlotMap;
    mutable std::mutex m_Mutex;
};

template <typename T>
template <typename ... Args>
constexpr DeviceSparseSet<T>::Handle DeviceSparseSet<T>::Insert(Args&&... args)
{
    std::lock_guard lock(m_Mutex);
    return m_SlotMap.insert(std::forward<Args>(args)...);
}

template <typename T>
constexpr void DeviceSparseSet<T>::Erase(Handle handle)
{
    std::lock_guard lock(m_Mutex);
    m_SlotMap.erase(handle);
}

template <typename T>
constexpr const T& DeviceSparseSet<T>::operator[](Handle handle) const
{
    std::lock_guard lock(m_Mutex);
    return m_SlotMap[handle];
}

template <typename T>
constexpr T& DeviceSparseSet<T>::operator[](Handle handle)
{
    return const_cast<T&>(const_cast<const DeviceSparseSet&>(*this)[handle]);
}

template <typename T>
constexpr void DeviceSparseSet<T>::Clear()
{
    std::lock_guard lock(m_Mutex);
    m_SlotMap.clear();
}
