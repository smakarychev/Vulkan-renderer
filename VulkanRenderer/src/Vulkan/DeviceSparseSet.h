#pragma once

#include "Rendering/ResourceHandle.h"

#include <CoreLib/Containers/PagedArray.h>
#include <CoreLib/Containers/SparseSet/SparseSetType.h>

template <typename T>
class DeviceSparseSet
{
    using Handle = GenerationalResourceHandle<typename T::ObjectType>;
    using HandleSparseSet = lux::SparseSetType<u32, Handle>;
    using Traits = lux::SparseSetGenerationTraits<Handle>;
    using ResourceSet = lux::PagedArray<T>;
public:
    using ValueType = T;
    
    template <typename ... Args>
    constexpr Handle Insert(Args&&... args);
    constexpr void Erase(Handle handle);

    constexpr const T& operator[](Handle handle) const;
    constexpr T& operator[](Handle handle);
    
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
constexpr DeviceSparseSet<T>::Handle DeviceSparseSet<T>::Insert(Args&&... args)
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
constexpr void DeviceSparseSet<T>::Erase(Handle handle)
{
    auto&& [gen, index] = Traits::Decompose(handle);

    std::lock_guard lock(m_Mutex);
    const u32 deletedIndex = m_SparseSet.erase(handle);
    m_Resources.erase(deletedIndex);
    m_FreeElements.push_back(Traits::Compose(gen + 1, index));
}

template <typename T>
constexpr const T& DeviceSparseSet<T>::operator[](Handle handle) const
{
    std::lock_guard lock(m_Mutex);
    u32 index = m_SparseSet.indexOf(handle);
    
    return m_Resources[index];
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
    m_Resources.Clear();
    m_SparseSet.Clear();
    m_FreeElements.clear();
}
