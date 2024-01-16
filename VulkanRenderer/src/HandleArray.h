#pragma once
#include <memory>
#include <vector>

#include "RenderHandle.h"

template <typename T>
class HandleArray
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

        Iterator(std::vector<std::unique_ptr<T>>* elements, IdType index)
            : m_Elements(elements), m_Index(index) {}
            
        reference operator*() const { return *m_Elements->operator[](m_Index); }
        pointer operator->() { return m_Elements->operator[](m_Index).get(); }
        Iterator operator++() { ++m_Index; return *this; }
        friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; }
        friend bool operator!=(const Iterator& a, const Iterator& b) { return !(a == b); }
    private:
        std::vector<std::unique_ptr<T>>* m_Elements;
        IdType m_Index;
    };
public:
    void Push(const T& val) { m_Elements.push_back(std::make_unique<T>(val)); }
    void Push(T&& val) { m_Elements.push_back(std::make_unique<T>(std::forward<T>(val))); }
    void push_back(const T& val) { m_Elements.push_back(std::make_unique<T>(val)); }
    void push_back(T&& val) { m_Elements.push_back(std::make_unique<T>(std::forward<T>(val))); }
    
    void Pop() { m_Elements.pop_back(); }
    void pop_back() { m_Elements.pop_back(); }

    u32 Size() const { return (u32)m_Elements.size(); }
    u32 size() const { return (u32)m_Elements.size(); }

    void Reserve(u32 size) { m_Elements.reserve(size); }
    void reserve(u32 size) { m_Elements.reserve(size); }

    void Resize(u32 size) { m_Elements.resize(size); }
    void resize(u32 size) { m_Elements.resize(size); }

    const T& operator[](RenderHandle<T> handle) const { return *m_Elements[Id(handle)]; }
    T& operator[](RenderHandle<T> handle) { return *m_Elements[Id(handle)]; }

    Iterator begin() { return Iterator(&m_Elements, 0); }
    Iterator end() { return Iterator(&m_Elements, (u32)m_Elements.size()); }
private:
    static IdType Id(RenderHandle<T> handle) { return handle.m_Id; }
private:
    std::vector<std::unique_ptr<T>> m_Elements;
};
