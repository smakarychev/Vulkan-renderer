#pragma once

#include "Common/DenseSetPaged.h"
#include "Common/SparseSet.h"

#include "Rendering/ResourceHandle.h"

template <typename T>
class DeviceSparseSet
{
    using OnResizeCallback = void (*)(T* oldMem, T* newMem);
    using OnSwapCallback = void (*)(T& a, T& b);

    using Handle = GenerationalResourceHandle<typename T::ObjectType>;
    using HandleSparseSet = SparseSet<u32, Handle>;
    using Traits = SparseSetGenerationTraits<Handle>;
    using ResourceSet = DenseSetPaged<T>;
public:
    using ValueType = T;
    
    template <typename ... Args>
    constexpr GenerationalResourceHandle<typename T::ObjectType> Add(Args&&... args);
    constexpr void Remove(GenerationalResourceHandle<typename T::ObjectType> handle);

    constexpr const T& operator[](GenerationalResourceHandle<typename T::ObjectType> handle) const;
    constexpr T& operator[](GenerationalResourceHandle<typename T::ObjectType> handle);
    
    constexpr u32 Count() const { return (u32)m_Resources.Size(); }
    constexpr u32 Capacity() const { return (u32)m_Resources.Capacity(); }
    
    constexpr void SetOnResizeCallback(OnResizeCallback) {}
    constexpr void SetOnSwapCallback(OnSwapCallback callback) { m_SwapCallback = callback; }
private:
    ResourceSet m_Resources;
    std::vector<Handle> m_FreeElements;
    HandleSparseSet m_SparseSet;
    OnSwapCallback m_SwapCallback = [](T&, T&){};
};

template <typename T>
template <typename ... Args>
constexpr GenerationalResourceHandle<typename T::ObjectType> DeviceSparseSet<T>::Add(Args&&... args)
{
    Handle handle = {};
    
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
    m_SparseSet.Push(handle);
    m_Resources.Push(std::forward<Args>(args)...);

    return handle;
}

template <typename T>
constexpr void DeviceSparseSet<T>::Remove(GenerationalResourceHandle<typename T::ObjectType> handle)
{
    auto&& [gen, index] = Traits::Decompose(handle);
    auto popCallback = [this, gen, index]()
    {
        m_FreeElements.push_back(Traits::Compose(gen + 1, index));
        m_Resources.Pop();
    };
    auto swapCallback = [this](u32 a, u32 b)
    {
        auto& resourceA = m_Resources[a];
        auto& resourceB = m_Resources[b];
        m_SwapCallback(resourceA, resourceB);
        std::swap(resourceA, resourceB);
    };
    m_SparseSet.Pop(handle, popCallback, swapCallback);
}

template <typename T>
constexpr const T& DeviceSparseSet<T>::operator[](GenerationalResourceHandle<typename T::ObjectType> handle) const
{
    u32 index = m_SparseSet.GetIndexOf(handle);
    
    return m_Resources[index];
}

template <typename T>
constexpr T& DeviceSparseSet<T>::operator[](GenerationalResourceHandle<typename T::ObjectType> handle)
{
    return const_cast<T&>(const_cast<const DeviceSparseSet&>(*this)[handle]);
}
