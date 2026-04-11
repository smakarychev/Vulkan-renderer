#pragma once

#include "Rendering/ResourceHandle.h"

#include <CoreLib/Containers/PagedArray.h>
#include <CoreLib/Containers/SparseSet/SparseSetType.h>

template <typename T>
class DeviceSparseSet
{
    using OnResizeCallback = void (*)(T* oldMem, T* newMem);
    using OnSwapCallback = void (*)(T& a, T& b);

    using Handle = GenerationalResourceHandle<typename T::ObjectType>;
    using HandleSparseSet = lux::SparseSetType<u32, Handle>;
    using Traits = lux::SparseSetGenerationTraits<Handle>;
    using ResourceSet = lux::PagedArray<T>;
public:
    using ValueType = T;
    
    template <typename ... Args>
    constexpr GenerationalResourceHandle<typename T::ObjectType> Add(Args&&... args);
    constexpr void Remove(GenerationalResourceHandle<typename T::ObjectType> handle);

    constexpr const T& operator[](GenerationalResourceHandle<typename T::ObjectType> handle) const;
    constexpr T& operator[](GenerationalResourceHandle<typename T::ObjectType> handle);
    
    constexpr u32 Count() const { return (u32)m_Resources.Size(); }
    constexpr u32 Capacity() const { return (u32)m_Resources.Capacity(); }

    constexpr void Clear();
private:
    ResourceSet m_Resources;
    std::vector<Handle> m_FreeElements;
    HandleSparseSet m_SparseSet;
    mutable std::mutex m_Mutex;
};

template <typename T>
template <typename ... Args>
constexpr GenerationalResourceHandle<typename T::ObjectType> DeviceSparseSet<T>::Add(Args&&... args)
{
    Handle handle = {};

    std::lock_guard lock(m_Mutex);
    if (!m_FreeElements.empty())
    {
        handle = m_FreeElements.back();
        auto&& [gen, index] = Traits::Decompose(handle);
        m_FreeElements.pop_back();
    }
    else
    {
        handle = Traits::Compose(0, m_Resources.Size());
    }
    m_SparseSet.insert(handle);
    m_Resources.insert(std::forward<Args>(args)...);

    return handle;
}

template <typename T>
constexpr void DeviceSparseSet<T>::Remove(GenerationalResourceHandle<typename T::ObjectType> handle)
{
    auto&& [gen, index] = Traits::Decompose(handle);

    std::lock_guard lock(m_Mutex);
    const u32 deletedIndex = m_SparseSet.erase(handle);
    m_Resources.erase(deletedIndex);
    m_FreeElements.push_back(Traits::Compose(gen + 1, index));
}

template <typename T>
constexpr const T& DeviceSparseSet<T>::operator[](GenerationalResourceHandle<typename T::ObjectType> handle) const
{
    std::lock_guard lock(m_Mutex);
    u32 index = m_SparseSet.indexOf(handle);
    
    return m_Resources[index];
}

template <typename T>
constexpr T& DeviceSparseSet<T>::operator[](GenerationalResourceHandle<typename T::ObjectType> handle)
{
    return const_cast<T&>(const_cast<const DeviceSparseSet&>(*this)[handle]);
}

template <typename T>
constexpr void DeviceSparseSet<T>::Clear()
{
    m_Resources.Clear();
    m_SparseSet.Clear();
    m_FreeElements.clear();
}
