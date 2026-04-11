#pragma once

#include <CoreLib/types.h>
#include <CoreLib/Math/CoreMath.h>

namespace lux
{
static constexpr u32 DENSE_SET_PAGE_SIZE = 256;
static_assert(Math::isPowerOf2(DENSE_SET_PAGE_SIZE), "Page size must be a power of 2");
static const u32 DENSE_SET_PAGE_SIZE_LOG = Math::log2(DENSE_SET_PAGE_SIZE);

// todo: this needs allocator support

template <typename T>
class DenseSetPaged
{
public:
    DenseSetPaged();

    template <typename... Args>
    constexpr u32 Insert(Args&&... args);
    template <typename... Args>
    constexpr u32 insert(Args&&... args) { return Insert(std::forward<Args>(args)...); }

    constexpr void Erase(u32 index);
    constexpr void erase(u32 index) { Erase(index); }
    
    constexpr void PopBack();
    constexpr void pop_back() { PopBack(); }

    constexpr const T& operator[](u32 index) const;
    constexpr T& operator[](u32 index);

    constexpr u32 Size() const { return m_Size; }
    constexpr u32 size() const { return Size(); }
    constexpr u32 Capacity() const { return (u32)m_Pages.size() * DENSE_SET_PAGE_SIZE; }
    constexpr u32 capacity() const { return Capacity(); }

    void Clear();
    void clear() { Clear(); }

private:
    constexpr std::vector<T>& GetOrCreatePage(u32 index);
    constexpr const std::vector<T>& GetPage(u32 index) const;
    constexpr std::vector<T>& GetPage(u32 index);

private:
    std::vector<std::vector<T>> m_Pages;
    u32 m_Size{0};
};

template <typename T>
DenseSetPaged<T>::DenseSetPaged()
{
    m_Pages.resize(1);
}

template <typename T>
void DenseSetPaged<T>::Clear()
{
    m_Pages.resize(1);
    m_Pages.front().clear();
}

template <typename T>
template <typename... Args>
constexpr u32 DenseSetPaged<T>::Insert(Args&&... args)
{
    std::vector<T>& page = GetOrCreatePage(m_Size);
    page.emplace_back(std::forward<Args>(args)...);
    const u32 oldSize = m_Size;
    m_Size++;

    return oldSize;
}

template <typename T>
constexpr void DenseSetPaged<T>::Erase(u32 index)
{
    ASSERT(m_Size > 0, "Cannot erase from empty set")
    m_Size--;

    auto& page = GetPage(m_Size);
    if (m_Size != 0)
    {
        using std::swap;
        swap((*this)[index], page.back());
    }
    
    page.back().~T();
    page.pop_back();
}

template <typename T>
constexpr void DenseSetPaged<T>::PopBack()
{
    ASSERT(m_Size > 0, "Cannot Pop from empty set")
    m_Size--;

    auto& page = GetPage(m_Size);
    page.back().~T();
    page.pop_back();
}

template <typename T>
constexpr const T& DenseSetPaged<T>::operator[](u32 index) const
{
    u32 indexMinor = Math::fastMod(index, DENSE_SET_PAGE_SIZE);
    const std::vector<T>& page = GetPage(index);
    ASSERT(page.capacity() != 0 && page.size() > indexMinor, "No element at index {}", index)

    return page[indexMinor];
}

template <typename T>
constexpr T& DenseSetPaged<T>::operator[](u32 index)
{
    return const_cast<T&>(const_cast<const DenseSetPaged&>(*this)[index]);
}

template <typename T>
constexpr std::vector<T>& DenseSetPaged<T>::GetOrCreatePage(u32 index)
{
    u32 pageNum = index >> DENSE_SET_PAGE_SIZE_LOG;
    if (pageNum >= m_Pages.size())
    {
        m_Pages.resize(pageNum + 1);
    }

    if (m_Pages[pageNum].capacity() == 0)
        m_Pages[pageNum].reserve(DENSE_SET_PAGE_SIZE);

    return m_Pages[pageNum];
}

template <typename T>
constexpr const std::vector<T>& DenseSetPaged<T>::GetPage(u32 index) const
{
    u32 pageNum = index >> DENSE_SET_PAGE_SIZE_LOG;
    ASSERT(pageNum < m_Pages.size() && m_Pages[pageNum].capacity() != 0, "No page at index {}", index)

    return m_Pages[pageNum];
}

template <typename T>
constexpr std::vector<T>& DenseSetPaged<T>::GetPage(u32 index)
{
    return const_cast<std::vector<T>&>(const_cast<const DenseSetPaged&>(*this).GetPage(index));
}
}
