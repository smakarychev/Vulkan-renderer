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

Sampler::Builder& Sampler::Builder::BorderColor(SamplerBorderColor color)
{
    m_CreateInfo.BorderColor = color;

    return *this;
}

Sampler::Builder& Sampler::Builder::ReductionMode(SamplerReductionMode mode)
{
    m_CreateInfo.ReductionMode = mode;

    return *this;
}

Sampler::Builder& Sampler::Builder::DepthCompareMode(SamplerDepthCompareMode mode)
{
    m_CreateInfo.DepthCompareMode = mode;

    return *this;
}

Sampler::Builder& Sampler::Builder::LodBias(f32 bias)
{
    m_CreateInfo.LodBias = bias;

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

u64 SamplerCache::SamplerKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hashKey =
        (u8)cacheKey.CreateInfo.MagnificationFilter |
        ((u8)cacheKey.CreateInfo.MagnificationFilter << 1) |
        (cacheKey.CreateInfo.WithAnisotropy << 2) |
        ((u8)cacheKey.CreateInfo.BorderColor << 3) |
        (cacheKey.CreateInfo.ReductionMode.has_value() << 4) |
        ((cacheKey.CreateInfo.ReductionMode.has_value() ? (u8)*cacheKey.CreateInfo.ReductionMode : 0) << 5) |
        ((u8)cacheKey.CreateInfo.DepthCompareMode << 6) |
        (std::hash<u32>{}(std::bit_cast<u32>(cacheKey.CreateInfo.LodBias)) ^
         std::hash<u32>{}(std::bit_cast<u32>(cacheKey.CreateInfo.MaxLod))) << 32;
    u64 hash = std::hash<u64>()(hashKey);
    
    return hash;
}

std::unordered_map<SamplerCache::CacheKey, Sampler, SamplerCache::SamplerKeyHash> SamplerCache::s_SamplerCache = {};