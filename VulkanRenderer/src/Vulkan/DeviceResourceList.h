#pragma once

#include "DeviceResourceListHandle.h"

#include <CoreLib/types.h>
#include <vector>

namespace lux
{
constexpr u32 DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE_LOG = 8;
constexpr u32 DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE = 1u << DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE_LOG;
static_assert(DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE_LOG == std::numeric_limits<u8>::digits);

struct DeviceResourceListColdDummy {};

template <typename Hot, typename Cold = DeviceResourceListColdDummy>
class DeviceResourceList
{
    static_assert(std::is_trivially_destructible_v<Hot>);
    using Handle = DeviceResourceListHandle<typename Hot::ObjectType>;
    using PageIndex = u32;
    using InPageIndex = u8;
    
    static constexpr auto HAS_COLD = !std::is_same_v<Cold, DeviceResourceListColdDummy>;
    
    using ColdDataPages = std::conditional_t<
        !HAS_COLD,
        DeviceResourceListColdDummy, 
        std::vector<std::vector<Cold>>>;
public:
    DeviceResourceList(std::string name)
        : m_Name(std::move(name))
    {
        static constexpr u32 INITIAL_PAGE_COUNT =
            std::numeric_limits<u16>::max() / DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE;
        static_assert(INITIAL_PAGE_COUNT < 256);
        
        m_HotDataPages.reserve(INITIAL_PAGE_COUNT);
        if constexpr(HAS_COLD)
            m_ColdDataPages.reserve(INITIAL_PAGE_COUNT);
    }
    DeviceResourceList() : DeviceResourceList("Unnamed")
    {
    }
    
    template <typename HotT, typename ColdT>
    requires HAS_COLD
    constexpr Handle Insert(HotT&& hot, ColdT&& cold)
    {
        std::scoped_lock lock(m_Mutex);
        
        const IndexInfo index = GetNewDataIndex();
        auto& hotData = m_HotDataPages[index.Page][index.InPage];
        hotData.Element.Element = std::forward<HotT>(hot);
        m_ColdDataPages[index.Page][index.InPage] = std::forward<ColdT>(cold);
        
        Handle handle;
        handle.m_Index = index.FlatIndex;
        handle.m_Generation = hotData.Generation;
        
        return handle;
    }
    
    template <typename HotT>
    requires !HAS_COLD
    constexpr Handle Insert(HotT&& hot)
    {
        std::scoped_lock lock(m_Mutex);
        
        const IndexInfo index = GetNewDataIndex();
        auto& hotData = m_HotDataPages[index.Page][index.InPage];
        hotData.Element.Element = std::forward<HotT>(hot);
        
        Handle handle;
        handle.m_Index = index.FlatIndex;
        handle.m_Generation = hotData.Generation;
        
        return handle;
    }
    
    constexpr void Erase(Handle handle)
    {
        std::scoped_lock lock(m_Mutex);
        
        const IndexInfo index = IndexInfo::FromFlatIndex((u16)handle.m_Index);
        ASSERT(index.Page < m_HotDataPages.size())
        auto& hotData = m_HotDataPages[index.Page][index.InPage];
        ASSERT(handle.m_Generation == hotData.Generation)
        
        hotData.Generation += 1;
        hotData.Element.NextFree = m_FirstFree;
        m_FirstFree = index.FlatIndex;
    }
    
    Hot& GetHot(Handle handle) requires HAS_COLD
    {
        const IndexInfo index = IndexInfo::FromFlatIndex((u16)handle.m_Index);
        ASSERT(index.Page < m_HotDataPages.size())
        auto& hotData = m_HotDataPages[index.Page][index.InPage];
        ASSERT(handle.m_Generation == hotData.Generation)
        
        return hotData.Element.Element;
    }
    
    Cold& GetCold(Handle handle) requires HAS_COLD
    {
        const IndexInfo index = IndexInfo::FromFlatIndex((u16)handle.m_Index);
        ASSERT(index.Page < m_HotDataPages.size())
        auto& hotData = m_HotDataPages[index.Page][index.InPage];
        ASSERT(handle.m_Generation == hotData.Generation)
        
        return m_ColdDataPages[index.Page][index.InPage];
    }
    
    std::pair<Hot*, Cold*> Get(Handle handle) requires HAS_COLD
    {
        const IndexInfo index = IndexInfo::FromFlatIndex((u16)handle.m_Index);
        ASSERT(index.Page < m_HotDataPages.size())
        auto& hotData = m_HotDataPages[index.Page][index.InPage];
        ASSERT(handle.m_Generation == hotData.Generation)
        
        return std::make_pair(&hotData.Element.Element, &m_ColdDataPages[index.Page][index.InPage]);
    }
    
    Hot& Get(Handle handle) requires !HAS_COLD
    {
        const IndexInfo index = IndexInfo::FromFlatIndex((u16)handle.m_Index);
        ASSERT(index.Page < m_HotDataPages.size())
        auto& hotData = m_HotDataPages[index.Page][index.InPage];
        ASSERT(handle.m_Generation == hotData.Generation)
        
        return hotData.Element.Element;
    }
private:
    struct IndexInfo
    {
        static constexpr u16 PAGE_SHIFT = DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE_LOG;
        static constexpr u16 IN_PAGE_MASK = (1u << PAGE_SHIFT) - 1u;
        
        PageIndex Page{};
        InPageIndex InPage{};
        u16 FlatIndex{};
        
        static constexpr IndexInfo FromFlatIndex(u16 freeIndex)
        {
            return {
                .Page = (PageIndex)(freeIndex >> PAGE_SHIFT),
                .InPage = (InPageIndex)(freeIndex & IN_PAGE_MASK),
                .FlatIndex = freeIndex,
            };
        }
    };
    constexpr IndexInfo GetNewDataIndex()
    {
        if (m_FirstFree != INVALID)
        {
            const IndexInfo index = IndexInfo::FromFlatIndex(m_FirstFree);
            m_FirstFree = m_HotDataPages[index.Page][index.InPage].Element.NextFree;
            
            return index;
        }
        
        return AllocateNewDataIndex();
    }
    constexpr IndexInfo AllocateNewDataIndex()
    {
        const PageIndex pageIndex = m_LastValidIndex >> DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE_LOG;
        const InPageIndex inPageIndex =
            (InPageIndex)(m_LastValidIndex & (DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE - 1));
        const u16 flatIndex = m_LastValidIndex;
        
        if (pageIndex >= m_HotDataPages.size())
        {
            ASSERT(pageIndex < m_HotDataPages.capacity(), "Too many elements in the list")
            if (pageIndex >= m_HotDataPages.capacity())
                LUX_LOG_ERROR("Reallocating DeviceResourceList {}, "
                              "this should not happen, consider increasing INITIAL_PAGE_COUNT", m_Name);
            
            m_HotDataPages.push_back({});
            m_HotDataPages[pageIndex].resize(DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE);
            if constexpr (HAS_COLD)
            {
                m_ColdDataPages.push_back({});
                m_ColdDataPages[pageIndex].resize(DEVICE_RESOURCE_LIST_PAGED_ARRAY_PAGE_SIZE);
            }
        }
        
        m_LastValidIndex += 1;
        
        return {pageIndex, inPageIndex, flatIndex};
    }
private:
    static constexpr u16 INVALID = (u16)~0u;
    template <typename T>
    struct DataWithGeneration
    {
        union Data
        {
            T Element;
            u16 NextFree{INVALID};
        };
        Data Element;
        u16 Generation{0};
    };
    
    std::vector<std::vector<DataWithGeneration<Hot>>> m_HotDataPages;
    [[no_unique_address]] ColdDataPages m_ColdDataPages;
    u16 m_FirstFree{INVALID};
    u16 m_LastValidIndex{0};
    std::mutex m_Mutex;
    std::string m_Name{};
};
}
