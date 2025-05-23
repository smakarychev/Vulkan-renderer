﻿#include "Descriptors.h"

#include "Vulkan/Device.h"

#include <algorithm>

std::unordered_map<DescriptorLayoutCache::CacheKey,
    DescriptorsLayout, DescriptorLayoutCache::DescriptorsLayoutKeyHash> DescriptorLayoutCache::s_LayoutCache = {};

DescriptorArenaAllocators::DescriptorArenaAllocators(DescriptorArenaAllocator samplerAllocator,
    DescriptorArenaAllocator resourceAllocator, DescriptorArenaAllocator materialAllocator)
    : m_Allocators({samplerAllocator, resourceAllocator, materialAllocator})
{
    ASSERT(Device::GetDescriptorArenaAllocatorKind(resourceAllocator) == DescriptorsKind::Resource,
        "Provided 'resource' allocator isn't actually a resource allocator")
    ASSERT(Device::GetDescriptorArenaAllocatorKind(samplerAllocator) == DescriptorsKind::Sampler,
        "Provided 'sampler' allocator isn't actually a sampler allocator")
    ASSERT(Device::GetDescriptorArenaAllocatorKind(materialAllocator) == DescriptorsKind::Materials,
        "Provided 'material' allocator isn't actually a resource allocator")
}

DescriptorArenaAllocator DescriptorArenaAllocators::Get(DescriptorsKind kind) const
{
    return m_Allocators[(u32)kind];
}

void DescriptorArenaAllocators::Reset(DescriptorsKind kind) const
{
    Device::ResetDescriptorArenaAllocator(m_Allocators[(u32)kind]);
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
