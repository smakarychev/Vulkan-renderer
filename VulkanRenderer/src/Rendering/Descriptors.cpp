#include "rendererpch.h"

#include "Descriptors.h"

#include "Shader/ShaderPipelineTemplate.h"
#include "Vulkan/Device.h"

std::unordered_map<DescriptorLayoutCache::CacheKey,
    DescriptorsLayout, DescriptorLayoutCache::DescriptorsLayoutKeyHash> DescriptorLayoutCache::s_LayoutCache = {};

DescriptorArenaAllocators::DescriptorArenaAllocators(Span<const DescriptorArenaAllocator> allocators)
{
    ASSERT(allocators.size() <= MAX_DESCRIPTOR_SETS)
    for (auto&& [i, allocator] : std::views::enumerate(allocators))
        m_Allocators[i] = allocator;
    m_AllocatorCount = (u32)allocators.size();
}

DescriptorArenaAllocator DescriptorArenaAllocators::Get(u32 index) const
{
    ASSERT(index < m_AllocatorCount);
    return m_Allocators[index];
}

void DescriptorArenaAllocators::ResetNonBindless() const
{
    for (u32 i = 0; i < m_AllocatorCount; i++)
        if (i != BINDLESS_DESCRIPTORS_INDEX)
            Device::ResetDescriptorArenaAllocator(m_Allocators[i]);
}

void DescriptorArenaAllocators::Reset(u32 index) const
{
    ASSERT(index < m_AllocatorCount);
    Device::ResetDescriptorArenaAllocator(m_Allocators[index]);
}

DescriptorLayoutCache::CacheKey DescriptorLayoutCache::CreateCacheKey(const DescriptorsLayoutCreateInfo& createInfo)
{
    CacheKey key = {};
    key.m_Flags = createInfo.Flags;
    key.m_Bindings.assign_range(createInfo.Bindings);
    key.m_BindingFlags.assign_range(createInfo.BindingFlags);
    SortBindings(key);

    return key;
}

DescriptorsLayout DescriptorLayoutCache::Find(const CacheKey& key)
{
    if (s_LayoutCache.contains(key))
        return s_LayoutCache.at(key);

    return {};
}

void DescriptorLayoutCache::Emplace(const CacheKey& key, DescriptorsLayout layout)
{
    s_LayoutCache.emplace(key, layout);
}

void DescriptorLayoutCache::SortBindings(CacheKey& cacheKey)
{
    std::sort(cacheKey.m_Bindings.begin(), cacheKey.m_Bindings.end(),
        [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
}

u64 DescriptorLayoutCache::DescriptorsLayoutKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hash = 0;
    for (auto& binding : cacheKey.m_Bindings)
    {
        u64 hashKey = binding.Binding | binding.Count << 8 | (u32)binding.Type << 16 | (u32)binding.Shaders << 24;
        hash ^= std::hash<u64>()(hashKey);
    }
    for (auto& bindingFlag : cacheKey.m_BindingFlags)
        hash ^= std::hash<u64>()((u64)bindingFlag);

    hash ^= std::hash<u64>()((u64)cacheKey.m_Flags);
    
    return hash;
}
