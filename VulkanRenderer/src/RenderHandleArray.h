#pragma once

#include "RenderHandle.h"

#include <vector>
#include <memory>

template <typename T>
class RenderHandleArray
{
    using IdType = typename RenderHandle<T>::UnderlyingType;
public:
    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = i32;
        using value_type = T;
        using pointer = value_type*;
        using reference = value_type&;

        constexpr Iterator(std::vector<T>* elements, IdType index)
            : m_Elements(elements), m_Index(index) {}
        constexpr Iterator(const std::vector<T>* elements, IdType index)
            : m_Elements(elements), m_Index(index) {}
            
        constexpr reference operator*() const { return m_Elements->operator[](m_Index); }
        constexpr pointer operator->() { return std::addressof(m_Elements->operator[](m_Index)); }
        constexpr Iterator& operator++() { ++m_Index; return *this; }
        constexpr Iterator operator++(i32) { Iterator tmp = *this; ++(*this); return tmp; }
        friend constexpr bool operator==(const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; }
        friend constexpr bool operator!=(const Iterator& a, const Iterator& b) { return !(a == b); }
    private:
        const std::vector<T>* m_Elements;
        IdType m_Index;
    };
    using iterator = Iterator;
public:
    constexpr RenderHandle<T> Push(const T& val);
    constexpr RenderHandle<T> Push(T&& val);
    constexpr RenderHandle<T> push_back(const T& val);
    constexpr RenderHandle<T> push_back(T&& val);

    constexpr void Pop() { m_Elements.pop_back(); }
    constexpr void pop_back() { Pop(); }

    constexpr RenderHandle<T> Insert(u32 position, const T& val);
    constexpr RenderHandle<T> Insert(u32 position, T&& val);
    constexpr RenderHandle<T> insert(u32 position, const T& val);
    constexpr RenderHandle<T> insert(u32 position, T&& val);

    constexpr u32 Size() const { return (u32)m_Elements.size(); }
    constexpr u32 size() const { return Size(); }

    constexpr bool Empty() const { return size() == 0; }
    constexpr bool empty() const { return Empty(); }

    constexpr void Reserve(u32 size) { m_Elements.reserve(size); }
    constexpr void reserve(u32 size) { Reserve(size); }

    constexpr void Resize(u32 size) { m_Elements.resize(size); }
    constexpr void resize(u32 size) { Resize(size); }

    constexpr const T& operator[](RenderHandle<T> handle) const { return m_Elements[Id(handle)]; }
    constexpr T& operator[](RenderHandle<T> handle) { return m_Elements[Id(handle)]; }

    constexpr Iterator begin() { return Iterator{&m_Elements, 0}; }
    constexpr Iterator end() { return Iterator{&m_Elements, (u32)m_Elements.size()}; }
    
    constexpr Iterator begin() const { return Iterator{&m_Elements, 0}; }
    constexpr Iterator end() const { return Iterator{&m_Elements, (u32)m_Elements.size()}; }

    constexpr IdType IndexOf(RenderHandle<T> handle) const { return Id(handle); }
    constexpr IdType index_of(RenderHandle<T> handle) const { return IndexOf(handle); }
private:
    constexpr static IdType Id(RenderHandle<T> handle) { return handle.m_Id; }
private:
    std::vector<T> m_Elements;
};

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::Push(const T& val)
{
    RenderHandle<T> toReturn = (u32)m_Elements.size();
    m_Elements.push_back(val);

    return toReturn;
}

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::Push(T&& val)
{
    RenderHandle<T> toReturn = (u32)m_Elements.size();
    m_Elements.push_back(std::forward<T>(val));

    return toReturn;
}

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::push_back(const T& val)
{
    return Push(val);
}

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::push_back(T&& val)
{
    return Push(std::forward<T>(val));
}

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::Insert(u32 position, const T& val)
{
    RenderHandle<T> toReturn = position;
    m_Elements.insert(m_Elements.begin() + position, val);

    return toReturn;
}

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::Insert(u32 position, T&& val)
{
    RenderHandle<T> toReturn = position;
    m_Elements.insert(m_Elements.begin() + position, std::forward<T>(val));

    return toReturn;
}

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::insert(u32 position, const T& val)
{
    return Insert(position, val);
}

template <typename T>
constexpr RenderHandle<T> RenderHandleArray<T>::insert(u32 position, T&& val)
{
    return Insert(position, std::forward<T>(val));
}
