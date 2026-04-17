#pragma once

#include "PagedDenseArray.h"
#include "SparseSet/SparseSetType.h"

namespace lux
{
template <typename Element, typename Handle>
class SlotMapType
{
    using HandleSparseSet = SparseSetType<u32, Handle>;
    using Traits = SparseSetGenerationTraits<Handle>;
public:
    template <bool IsConst>
    struct Iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = i32;
        using value_type = Element;
        using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;
        using reference = std::conditional_t<IsConst, const value_type&, value_type&>;

        Iterator(const PagedDenseArray<Element>& dense, u32 index)
            : m_Index(index), m_Dense(&dense)
        {
        }

        reference operator*() const { return (*m_Dense)[m_Index]; }
        pointer operator->() { return &(*m_Dense)[m_Index]; }

        Iterator operator++()
        {
            m_Index--;
            return *this;
        }

        friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_Index == b.m_Index; }
        friend bool operator!=(const Iterator& a, const Iterator& b) { return !(a == b); }
        
        template<bool OtherConst>
        friend bool operator==(const Iterator& a, const Iterator<OtherConst>& b) { return a.m_Index == b.m_Index; }
    
        template<bool OtherConst>
        friend bool operator!=(const Iterator& a, const Iterator<OtherConst>& b) { return !(a == b); }
    private:
        u32 m_Index;
        std::conditional_t<IsConst, const PagedDenseArray<Element>*, PagedDenseArray<Element>*> m_Dense;
    };
public:
    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;
    
    template <typename... Args>
    constexpr Handle Insert(Args&&... args);
    template <typename... Args>
    constexpr Handle insert(Args&&... args) { return Insert(std::forward<Args>(args)...); }
    
    constexpr void Erase(Handle handle);
    constexpr void erase(Handle handle) { return Erase(handle); }

    constexpr const Element& operator[](Handle handle) const;
    constexpr Element& operator[](Handle handle);
    
    constexpr u32 Size() const { return (u32)m_Elements.size(); }
    constexpr u32 size() const { return Size(); }
    
    constexpr u32 Capacity() const { return (u32)m_Elements.capacity(); }
    constexpr u32 capacity() const { return Capacity(); }

    constexpr void Clear();
    constexpr void clear() { return Clear(); }
    
    constexpr iterator begin() { return iterator(m_Elements, (u32)(m_Elements.size() - 1)); }
    constexpr iterator end() { return iterator(m_Elements, -1); }
    constexpr const_iterator begin() const { return const_iterator(m_Elements, (u32)(m_Elements.size() - 1)); }
    constexpr const_iterator end() const { return const_iterator(m_Elements, -1); }
private:
    PagedDenseArray<Element> m_Elements;
    HandleSparseSet m_SparseSet;
    // todo: store as embedded freelist somewhere?
    std::vector<Handle> m_FreeElements;
};

template <typename Element, typename Handle>
template <typename ... Args>
constexpr Handle SlotMapType<Element, Handle>::Insert(Args&&... args)
{
    Handle index;
    if (!m_FreeElements.empty())
    {
        index = m_FreeElements.back();
        m_FreeElements.pop_back();
    }
    else
    {
        index = Traits::Compose(0, m_Elements.size());
    }
    m_SparseSet.insert(index);
    m_Elements.insert(std::forward<Args>(args)...);

    return index;
}

template <typename Element, typename Handle>
constexpr void SlotMapType<Element, Handle>::Erase(Handle handle)
{
    auto&& [gen, index] = Traits::Decompose(handle);
    const u32 deletedIndex = m_SparseSet.erase(handle);
    m_Elements.erase(deletedIndex);
    m_FreeElements.push_back(Traits::Compose(gen + 1, index));
}

template <typename Element, typename Handle>
constexpr const Element& SlotMapType<Element, Handle>::operator[](Handle handle) const
{
    return m_Elements[m_SparseSet.index_of(handle)];
}

template <typename Element, typename Handle>
constexpr Element& SlotMapType<Element, Handle>::operator[](Handle handle)
{
    return const_cast<Element&>(const_cast<const SlotMapType&>(*this)[handle]);
}

template <typename Element, typename Handle>
constexpr void SlotMapType<Element, Handle>::Clear()
{
    m_Elements.Clear();
    m_SparseSet.Clear();
    m_FreeElements.clear();
}

template <typename Element>
using SlotMap = SlotMapType<Element, u32>;
}
