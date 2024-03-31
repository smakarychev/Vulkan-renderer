#include "Sampler.h"

#include "Vulkan/Driver.h"

Sampler Sampler::Builder::Build()
{
    return SamplerCache::CreateSampler(m_CreateInfo);
}

Sampler::Builder& Sampler::Builder::Filters(ImageFilter minification, ImageFilter magnification)
{
    m_CreateInfo.MinificationFilter = minification;
    m_CreateInfo.MagnificationFilter = magnification;

    return *this;
}

Sampler::Builder& Sampler::Builder::WrapMode(SamplerWrapMode mode)
{
    m_CreateInfo.AddressMode = mode;

    return *this;
}

Sampler::Builder& Sampler::Builder::ReductionMode(SamplerReductionMode mode)
{
    m_CreateInfo.ReductionMode = mode;

    return *this;
}

Sampler::Builder& Sampler::Builder::MaxLod(f32 lod)
{
    m_CreateInfo.MaxLod = lod;
    
    return *this;
}

Sampler::Builder& Sampler::Builder::WithAnisotropy(bool enabled)
{
    m_CreateInfo.WithAnisotropy = enabled;

    return *this;
}

Sampler Sampler::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void Sampler::Destroy(const Sampler& sampler)
{
    Driver::Destroy(sampler.Handle());
}


Sampler SamplerCache::CreateSampler(const Sampler::Builder::CreateInfo& createInfo)
{
    CacheKey key = {.CreateInfo = createInfo};

    if (s_SamplerCache.contains(key))
        return s_SamplerCache.at(key);

    Sampler newSampler = Sampler::Create(createInfo);
    s_SamplerCache.emplace(key, newSampler);
    
    Driver::DeletionQueue().Enqueue(newSampler);

    return newSampler;
}

bool SamplerCache::CacheKey::operator==(const CacheKey& other) const
{
    return
        CreateInfo.MinificationFilter == other.CreateInfo.MinificationFilter &&
        CreateInfo.MagnificationFilter == other.CreateInfo.MagnificationFilter &&
        CreateInfo.ReductionMode == other.CreateInfo.ReductionMode &&
        CreateInfo.MaxLod == other.CreateInfo.MaxLod &&
        CreateInfo.WithAnisotropy == other.CreateInfo.WithAnisotropy;
}

u64 SamplerCache::SamplerKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hashKey =
        (u32)cacheKey.CreateInfo.MagnificationFilter |
        ((u32)cacheKey.CreateInfo.MagnificationFilter << 1) |
        (cacheKey.CreateInfo.WithAnisotropy << 2) |
        (cacheKey.CreateInfo.ReductionMode.has_value() << 3) |
        ((cacheKey.CreateInfo.ReductionMode.has_value() ? (u32)*cacheKey.CreateInfo.ReductionMode : 0) << 4) |
        ((u64)std::bit_cast<u32>(cacheKey.CreateInfo.MaxLod) << 32);
    u64 hash = std::hash<u64>()(hashKey);
    
    return hash;
}

std::unordered_map<SamplerCache::CacheKey, Sampler, SamplerCache::SamplerKeyHash> SamplerCache::s_SamplerCache = {};