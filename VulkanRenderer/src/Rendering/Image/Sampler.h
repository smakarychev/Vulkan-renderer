#pragma once

#include "ImageTraits.h"
#include "types.h"
#include "Rendering/ResourceHandle.h"

#include <optional>
#include <unordered_map>

struct SamplerCreateInfo
{
    static constexpr f32 LOD_MAX = 1000.0f;

    ImageFilter MinificationFilter{ImageFilter::Linear};
    ImageFilter MagnificationFilter{ImageFilter::Linear};
    SamplerWrapMode WrapMode{SamplerWrapMode::Repeat};
    SamplerBorderColor BorderColor{SamplerBorderColor::White};
    std::optional<SamplerReductionMode> ReductionMode{};
    SamplerDepthCompareMode DepthCompareMode{SamplerDepthCompareMode::None};
    f32 LodBias{0.0f};
    f32 MaxLod{LOD_MAX};
    bool WithAnisotropy{true};
};

class Sampler
{
    FRIEND_INTERNAL
    friend class Image;
    friend class SamplerCache;
public:
    static void Destroy(const Sampler& sampler);
private:
    ResourceHandleType<Sampler> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<Sampler> m_ResourceHandle{};
};

class SamplerCache
{
public:
    class CacheKey
    {
        friend class SamplerCache;
    public:
        auto operator<=>(const CacheKey&) const = default;
    private:
        ImageFilter m_MinificationFilter{ImageFilter::Linear};
        ImageFilter m_MagnificationFilter{ImageFilter::Linear};
        SamplerWrapMode m_WrapMode{SamplerWrapMode::Repeat};
        SamplerBorderColor m_BorderColor{SamplerBorderColor::White};
        std::optional<SamplerReductionMode> m_ReductionMode{};
        SamplerDepthCompareMode m_DepthCompareMode{SamplerDepthCompareMode::None};
        f32 m_LodBias{0.0f};
        f32 m_MaxLod{SamplerCreateInfo::LOD_MAX};
        bool m_WithAnisotropy{true};
    };
public:
    static CacheKey CreateCacheKey(const SamplerCreateInfo& createInfo);
    static Sampler* Find(const CacheKey& key);
    static void Emplace(const CacheKey& key, Sampler sampler);
private:
    struct SamplerKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };

    static std::unordered_map<CacheKey, Sampler, SamplerKeyHash> s_SamplerCache;
};