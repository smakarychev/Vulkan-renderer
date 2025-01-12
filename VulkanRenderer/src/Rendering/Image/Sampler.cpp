#include "Sampler.h"

#include "Vulkan/Device.h"

void Sampler::Destroy(const Sampler& sampler)
{
    Device::Destroy(sampler.Handle());
}

SamplerCache::CacheKey SamplerCache::CreateCacheKey(const SamplerCreateInfo& createInfo)
{
    CacheKey key = {};
    key.m_MinificationFilter = createInfo.MinificationFilter;
    key.m_MagnificationFilter = createInfo.MagnificationFilter;
    key.m_WrapMode = createInfo.WrapMode;
    key.m_BorderColor = createInfo.BorderColor;
    key.m_ReductionMode = createInfo.ReductionMode;
    key.m_DepthCompareMode = createInfo.DepthCompareMode;
    key.m_LodBias = createInfo.LodBias;
    key.m_MaxLod = createInfo.MaxLod;
    key.m_WithAnisotropy = createInfo.WithAnisotropy;

    return key;
}

Sampler* SamplerCache::Find(const CacheKey& key)
{
    if (s_SamplerCache.contains(key))
        return &s_SamplerCache.at(key);

    return nullptr;
}

void SamplerCache::Emplace(const CacheKey& key, Sampler sampler)
{
    s_SamplerCache.emplace(key, sampler);
}

u64 SamplerCache::SamplerKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hashKey =
        (u8)cacheKey.m_MagnificationFilter |
        ((u8)cacheKey.m_MagnificationFilter << 1) |
        ((u8)cacheKey.m_WrapMode << 2) |
        ((u8)cacheKey.m_BorderColor << 3) |
        (cacheKey.m_ReductionMode.has_value() << 4) |
        ((cacheKey.m_ReductionMode.has_value() ? (u8)*cacheKey.m_ReductionMode : 0) << 5) |
        ((u8)cacheKey.m_DepthCompareMode << 6) |
        (cacheKey.m_WithAnisotropy << 7) |
        (std::hash<u32>{}(std::bit_cast<u32>(cacheKey.m_LodBias)) ^
         std::hash<u32>{}(std::bit_cast<u32>(cacheKey.m_MaxLod))) << 32;
    u64 hash = std::hash<u64>()(hashKey);
    
    return hash;
}

std::unordered_map<SamplerCache::CacheKey, Sampler, SamplerCache::SamplerKeyHash> SamplerCache::s_SamplerCache = {};