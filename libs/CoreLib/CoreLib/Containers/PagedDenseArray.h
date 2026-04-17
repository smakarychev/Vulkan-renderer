#pragma once

#include <CoreLib/types.h>
#include <CoreLib/Math/CoreMath.h>

namespace lux
{
// todo: this needs allocator support

static constexpr u32 DEFAULT_PAGE_SIZE_LOG = 8;

template <typename T, u32 PageSizeLog = DEFAULT_PAGE_SIZE_LOG>
class PagedDenseArray
{
public:
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
    constexpr u32 Capacity() const { return (u32)m_Pages.size() * PAGE_SIZE; }
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

    static constexpr u32 PAGE_SIZE = 1u << PageSizeLog;
};

template <typename T, u32 PageSizeLog>
void PagedDenseArray<T, PageSizeLog>::Clear()
{
    m_Pages.clear();
}

template <typename T, u32 PageSizeLog>
template <typename... Args>
constexpr u32 PagedDenseArray<T, PageSizeLog>::Insert(Args&&... args)
{
    std::vector<T>& page = GetOrCreatePage(m_Size);
    page.emplace_back(std::forward<Args>(args)...);
    const u32 insertedIndex = m_Size;
    m_Size++;

    return insertedIndex;
}

template <typename T, u32 PageSizeLog>
constexpr void PagedDenseArray<T, PageSizeLog>::Erase(u32 index)
{
    ASSERT(m_Size > 0, "Cannot erase from empty set")

    auto& page = GetPage(m_Size - 1);
    if (m_Size > 1)
    {
        using std::swap;
        swap((*this)[index], page.back());
    }
    
    page.back().~T();
    page.pop_back();
    
    m_Size--;
}

template <typename T, u32 PageSizeLog>
constexpr void PagedDenseArray<T, PageSizeLog>::PopBack()
{
    ASSERT(m_Size > 0, "Cannot Pop from empty set")

    auto& page = GetPage(m_Size - 1);
    page.back().~T();
    page.pop_back();

    m_Size--;
}

template <typename T, u32 PageSizeLog>
constexpr const T& PagedDenseArray<T, PageSizeLog>::operator[](u32 index) const
{
    ASSERT(index < size(), "No element at index {}", index)
    u32 indexMinor = Math::fastMod(index, PAGE_SIZE);
    const std::vector<T>& page = GetPage(index);

    return page[indexMinor];
}

template <typename T, u32 PageSizeLog>
constexpr T& PagedDenseArray<T, PageSizeLog>::operator[](u32 index)
{
    return const_cast<T&>(const_cast<const PagedDenseArray&>(*this)[index]);
}

template <typename T, u32 PageSizeLog>
constexpr std::vector<T>& PagedDenseArray<T, PageSizeLog>::GetOrCreatePage(u32 index)
{
    u32 pageNum = index >> PageSizeLog;
    if (pageNum >= m_Pages.size())
    {
        m_Pages.resize(pageNum + 1);
        m_Pages[pageNum].reserve(PAGE_SIZE);
    }

    return m_Pages[pageNum];
}

template <typename T, u32 PageSizeLog>
constexpr const std::vector<T>& PagedDenseArray<T, PageSizeLog>::GetPage(u32 index) const
{
    u32 pageNum = index >> PageSizeLog;
    ASSERT(pageNum < m_Pages.size() && m_Pages[pageNum].capacity() != 0, "No page at index {}", index)

    return m_Pages[pageNum];
}

template <typename T, u32 PageSizeLog>
constexpr std::vector<T>& PagedDenseArray<T, PageSizeLog>::GetPage(u32 index)
{
    return const_cast<std::vector<T>&>(const_cast<const PagedDenseArray&>(*this).GetPage(index));
}
}
