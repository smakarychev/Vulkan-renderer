#pragma once
#include <optional>
#include <unordered_map>

#include "ImageTraits.h"
#include "types.h"
#include "Rendering/DriverResourceHandle.h"

class Sampler
{
    FRIEND_INTERNAL
    friend class Image;
    friend class SamplerCache;
public:
    static constexpr f32 LOD_MAX = 1000.0f;
    
    class Builder
    {
        friend class Sampler;
        friend class SamplerCache;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            ImageFilter MinificationFilter{ImageFilter::Linear};
            ImageFilter MagnificationFilter{ImageFilter::Linear};
            SamplerWrapMode AddressMode{SamplerWrapMode::Repeat};
            SamplerBorderColor BorderColor{SamplerBorderColor::White};
            std::optional<SamplerReductionMode> ReductionMode;
            f32 MaxLod{LOD_MAX};
            bool WithAnisotropy{true};
        };
    public:
        Sampler Build();
        Builder& Filters(ImageFilter minification, ImageFilter magnification);
        Builder& WrapMode(SamplerWrapMode mode);
        Builder& BorderColor(SamplerBorderColor color);
        Builder& ReductionMode(SamplerReductionMode mode);
        Builder& MaxLod(f32 lod);
        Builder& WithAnisotropy(bool enabled);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Sampler Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Sampler& sampler);
private:
    ResourceHandle<Sampler> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandle<Sampler> m_ResourceHandle{};
};

class SamplerCache
{
public:
    static Sampler CreateSampler(const Sampler::Builder::CreateInfo& createInfo);
private:
    struct CacheKey
    {
        Sampler::Builder::CreateInfo CreateInfo;
        bool operator==(const CacheKey& other) const;
    };
    struct SamplerKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };

    static std::unordered_map<CacheKey, Sampler, SamplerKeyHash> s_SamplerCache;
};