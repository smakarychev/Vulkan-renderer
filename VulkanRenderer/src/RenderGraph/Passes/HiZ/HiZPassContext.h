#pragma once
#include "RenderGraph/RGCommon.h"

enum class HiZReductionMode { Min = 0, Max = 1, MaxVal };

class HiZPassContext
{
public:
    static constexpr u16 MAX_MIPMAP_COUNT = 16;
    HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue);

    void SetHiZResource(RG::Resource hiz, HiZReductionMode mode) { m_HiZResources[(u32)mode] = hiz; }
    RG::Resource GetHiZResource(HiZReductionMode mode) const { return m_HiZResources[(u32)mode]; }
    const Texture& GetHiZ(HiZReductionMode mode) const { return m_HiZs[(u32)mode]; }
    std::shared_ptr<Texture>* GetHiZPrevious(HiZReductionMode mode) { return &m_HiZsPrevious[(u32)mode]; }
    const Texture* GetHiZPrevious(HiZReductionMode mode) const { return m_HiZsPrevious[(u32)mode].get(); }
    Sampler GetMinMaxSampler(HiZReductionMode mode) const { return m_MinMaxSamplers[(u32)mode]; }
    const std::vector<ImageViewHandle>& GetViewHandles() const { return m_MipmapViewHandles; }

    /* NOTE: this is not hiz resolution (which is a power of 2), but the resolution that was passed into constructor */
    const glm::uvec2& GetDrawResolution() const { return m_DrawResolution; }
    const glm::uvec2& GetHiZResolution() const { return m_HiZResolution; }
private:
    std::array<RG::Resource, (u32)HiZReductionMode::MaxVal> m_HiZResources;
    std::array<Texture, (u32)HiZReductionMode::MaxVal> m_HiZs;
    std::array<std::shared_ptr<Texture>, (u32)HiZReductionMode::MaxVal> m_HiZsPrevious;
    std::array<Sampler, (u32)HiZReductionMode::MaxVal> m_MinMaxSamplers;
    
    std::vector<ImageViewHandle> m_MipmapViewHandles;
    
    glm::uvec2 m_DrawResolution{};
    glm::uvec2 m_HiZResolution{};
};
