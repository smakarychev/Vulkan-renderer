#pragma once
#include <memory>
#include <vector>

#include "RenderHandle.h"

template <typename T>
class HandleArray
{
    using IdType = typename RenderHandle<T>::UnderlyingType;
public:
    void Push(const T& val) { m_Elements.push_back(std::make_unique<T>(val)); }
    void Push(T&& val) { m_Elements.push_back(std::make_unique<T>(std::forward<T>(val))); }
    void push_back(const T& val) { m_Elements.push_back(std::make_unique<T>(val)); }
    void push_back(T&& val) { m_Elements.push_back(std::make_unique<T>(std::forward<T>(val))); }
    
    void Pop() { m_Elements.pop_back(); }
    void pop_back() { m_Elements.pop_back(); }

    u32 Size() { return (u32)m_Elements.size(); }
    u32 size() { return (u32)m_Elements.size(); }

    void Reserve(u32 size) { m_Elements.reserve(size); }
    void reserve(u32 size) { m_Elements.reserve(size); }

    void Resize(u32 size) { m_Elements.resize(size); }
    void resize(u32 size) { m_Elements.resize(size); }

    const T& operator[](RenderHandle<T> handle) const { return *m_Elements[Id(handle)]; }
    T& operator[](RenderHandle<T> handle) { return *m_Elements[Id(handle)]; }

    
private:
    static IdType Id(RenderHandle<T> handle) { return handle.m_Id; }
private:
    std::vector<std::unique_ptr<T>> m_Elements;
};
