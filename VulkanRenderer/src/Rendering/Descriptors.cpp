#include "rendererpch.h"

#include "Descriptors.h"

#include "Shader/ShaderPipelineTemplate.h"
#include "Vulkan/Device.h"

std::unordered_map<DescriptorLayoutCache::CacheKey,
    DescriptorsLayout, DescriptorLayoutCache::DescriptorsLayoutKeyHash> DescriptorLayoutCache::s_LayoutCache = {};

DescriptorArenaAllocators::DescriptorArenaAllocators(Span<const DescriptorArenaAllocator> transientAllocators,
    DescriptorArenaAllocator persistentAllocator)
{
    ASSERT(transientAllocators.size() <= MAX_DESCRIPTOR_SETS)
    for (auto&& [i, allocator] : std::views::enumerate(transientAllocators))
        m_TransientAllocators[i] = allocator;
    m_TransientDescriptorAllocators = (u32)transientAllocators.size();

    m_PersistentAllocator = persistentAllocator;
}

DescriptorArenaAllocator DescriptorArenaAllocators::GetTransient(u32 index) const
{
    ASSERT(index < m_TransientDescriptorAllocators)
    return m_TransientAllocators[index];
}

DescriptorArenaAllocator DescriptorArenaAllocators::GetPersistent() const
{
    return m_PersistentAllocator;
}

void DescriptorArenaAllocators::ResetTransient() const
{
    for (u32 i = 0; i < m_TransientDescriptorAllocators; i++)
        Device::ResetDescriptorArenaAllocator(m_TransientAllocators[i]);
}

void DescriptorArenaAllocators::Reset(u32 index) const
{
    ASSERT(index < m_TransientDescriptorAllocators);
    Device::ResetDescriptorArenaAllocator(m_TransientAllocators[index]);
}

DescriptorLayoutCache::CacheKey DescriptorLayoutCache::CreateCacheKey(const DescriptorsLayoutCreateInfo& createInfo)
{
    CacheKey key = {};
    key.m_Flags = createInfo.Flags;
    key.m_Bindings.assign_range(createInfo.Bindings);
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
        u64 hashKey = binding.Binding | binding.Count << 8 | (u32)binding.Type << 16 | (u32)binding.Shaders << 24 |
            (u64)binding.Flags << 32;
        hash ^= std::hash<u64>()(hashKey);
    }

    hash ^= std::hash<u64>()((u64)cacheKey.m_Flags);
    
    return hash;
}

u32 descriptors::safeBindlessCountForDescriptorType(DescriptorType type)
{
    /* divide max count by 2 to allow for non-bindless resources in pipeline */
    switch (type)
    {
    case DescriptorType::Sampler:
    case DescriptorType::Image:
    case DescriptorType::ImageStorage:
        return Device::GetMaxIndexingImages() >> 1;
    case DescriptorType::UniformBuffer:
        return Device::GetMaxIndexingUniformBuffers() >> 1;
    case DescriptorType::StorageBuffer:
        return Device::GetMaxIndexingStorageBuffers() >> 1;
    case DescriptorType::UniformBufferDynamic:
        return Device::GetMaxIndexingUniformBuffersDynamic() >> 1;
    case DescriptorType::StorageBufferDynamic:
        return Device::GetMaxIndexingStorageBuffersDynamic() >> 1;
    default:
        ASSERT(false, "Unsupported descriptor bindless type")
        return 0;
    }
}
