#pragma once

#include "types.h"
#include "utils/MathUtils.h"

static constexpr u32 DENSE_SET_PAGE_SIZE = 256;
static_assert(MathUtils::isPowerOf2(DENSE_SET_PAGE_SIZE), "Page size must be a power of 2");
static const u32 DENSE_SET_PAGE_SIZE_LOG = MathUtils::log2(DENSE_SET_PAGE_SIZE);

template <typename T>
class DenseSetPaged
{
public:
    DenseSetPaged();

    template <typename ... Args>
    constexpr void Push(Args&&... args);

    constexpr void Pop();
    
    constexpr void UnorderedRemove(u32 index);
    
    constexpr const T& operator[](u32 index) const;
    constexpr T& operator[](u32 index);

    constexpr u32 Size() const { return m_Size; }
    constexpr u32 Capacity() const { return (u32)m_Pages.size() * DENSE_SET_PAGE_SIZE; }
private:
    constexpr std::vector<T>& GetOrCreatePage(u32 index);
    constexpr const std::vector<T>& GetPage(u32 index) const;
    constexpr std::vector<T>& GetPage(u32 index);
private:
    std::vector<std::vector<T>> m_Pages;
    u32 m_Size{0};
    u32 m_LastNonEmptyPage{0};
};

template <typename T>
DenseSetPaged<T>::DenseSetPaged()
{
    m_Pages.resize(1);
}

template <typename T>
template <typename ... Args>
constexpr void DenseSetPaged<T>::Push(Args&&... args)
{
    std::vector<T>& page = GetOrCreatePage(m_Size);
    page.emplace_back(std::forward<Args>(args)...);
    m_Size++;

    if ((m_Size >> DENSE_SET_PAGE_SIZE_LOG) >= m_LastNonEmptyPage)
        m_LastNonEmptyPage++;
}

template <typename T>
constexpr void DenseSetPaged<T>::Pop()
{
    ASSERT(m_Size > 0, "Cannot Pop from empty set")
    m_Size--;

    m_LastNonEmptyPage = (m_Size >> DENSE_SET_PAGE_SIZE_LOG);
    
    m_Pages[m_LastNonEmptyPage].back().~T();
    m_Pages[m_LastNonEmptyPage].pop_back();

    for (u32 i = m_LastNonEmptyPage + 1; i < m_Pages.size(); i++)
        ASSERT(m_Pages[i].empty())
}

template <typename T>
constexpr void DenseSetPaged<T>::UnorderedRemove(u32 index)
{
    if (m_Size > 1)
        std::swap((*this)[index], m_Pages[m_LastNonEmptyPage].back());
    Pop();
}

template <typename T>
constexpr const T& DenseSetPaged<T>::operator[](u32 index) const
{
    u32 indexMinor = MathUtils::fastMod(index, DENSE_SET_PAGE_SIZE);
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
    return const_cast<std::vector<T>&>(const_cast<DenseSetPaged<T>&>(*this).GetPage(index));
}
