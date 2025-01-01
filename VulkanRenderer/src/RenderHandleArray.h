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

        Iterator(std::vector<T>* elements, IdType index)
            : m_Elements(elements), m_Index(index) {}
        Iterator(const std::vector<T>* elements, IdType index)
            : m_Elements(elements), m_Index(index) {}
            
        reference operator*() const { return m_Elements->operator[](m_Index); }
        pointer operator->() { return std::addressof(m_Elements->operator[](m_Index)); }
        Iterator& operator++() { ++m_Index; return *this; }
        Iterator operator++(i32) { Iterator tmp = *this; ++(*this); return tmp; }
        friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; }
        friend bool operator!=(const Iterator& a, const Iterator& b) { return !(a == b); }
    private:
        const std::vector<T>* m_Elements;
        IdType m_Index;
    };
    using iterator = Iterator;
public:
    RenderHandle<T> Push(const T& val);
    RenderHandle<T> Push(T&& val);
    RenderHandle<T> push_back(const T& val);
    RenderHandle<T> push_back(T&& val);

    void Pop() { m_Elements.pop_back(); }
    void pop_back() { Pop(); }

    RenderHandle<T> Insert(u32 position, const T& val);
    RenderHandle<T> Insert(u32 position, T&& val);
    RenderHandle<T> insert(u32 position, const T& val);
    RenderHandle<T> insert(u32 position, T&& val);

    u32 Size() const { return (u32)m_Elements.size(); }
    u32 size() const { return Size(); }

    bool Empty() const { return size() == 0; }
    bool empty() const { return Empty(); }

    void Reserve(u32 size) { m_Elements.reserve(size); }
    void reserve(u32 size) { Reserve(size); }

    void Resize(u32 size) { m_Elements.resize(size); }
    void resize(u32 size) { Resize(size); }

    const T& operator[](RenderHandle<T> handle) const { return m_Elements[Id(handle)]; }
    T& operator[](RenderHandle<T> handle) { return m_Elements[Id(handle)]; }

    Iterator begin() { return Iterator{&m_Elements, 0}; }
    Iterator end() { return Iterator{&m_Elements, (u32)m_Elements.size()}; }
    
    Iterator begin() const { return Iterator{&m_Elements, 0}; }
    Iterator end() const { return Iterator{&m_Elements, (u32)m_Elements.size()}; }

    IdType IndexOf(RenderHandle<T> handle) const { return Id(handle); }
    IdType index_of(RenderHandle<T> handle) const { return IndexOf(handle); }
private:
    static IdType Id(RenderHandle<T> handle) { return handle.m_Id; }
private:
    std::vector<T> m_Elements;
};

template <typename T>
RenderHandle<T> RenderHandleArray<T>::Push(const T& val)
{
    RenderHandle<T> toReturn = (u32)m_Elements.size();
    m_Elements.push_back(val);

    return toReturn;
}

template <typename T>
RenderHandle<T> RenderHandleArray<T>::Push(T&& val)
{
    RenderHandle<T> toReturn = (u32)m_Elements.size();
    m_Elements.push_back(std::forward<T>(val));

    return toReturn;
}

template <typename T>
RenderHandle<T> RenderHandleArray<T>::push_back(const T& val)
{
    return Push(val);
}

template <typename T>
RenderHandle<T> RenderHandleArray<T>::push_back(T&& val)
{
    return Push(std::forward<T>(val));
}

template <typename T>
RenderHandle<T> RenderHandleArray<T>::Insert(u32 position, const T& val)
{
    RenderHandle<T> toReturn = position;
    m_Elements.insert(m_Elements.begin() + position, val);

    return toReturn;
}

template <typename T>
RenderHandle<T> RenderHandleArray<T>::Insert(u32 position, T&& val)
{
    RenderHandle<T> toReturn = position;
    m_Elements.insert(m_Elements.begin() + position, std::forward<T>(val));

    return toReturn;
}

template <typename T>
RenderHandle<T> RenderHandleArray<T>::insert(u32 position, const T& val)
{
    return Insert(position, val);
}

template <typename T>
RenderHandle<T> RenderHandleArray<T>::insert(u32 position, T&& val)
{
    return Insert(position, std::forward<T>(val));
}
