#pragma once
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"

class HiZPassContext
{
public:
    static constexpr u16 MAX_MIPMAP_COUNT = 16;
    HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue);

    void SetHiZResource(RG::Resource hiz) { m_HiZResource = hiz; }
    RG::Resource GetHiZResource() const { return m_HiZResource; }
    const Texture& GetHiZ() const { return m_HiZ; }
    std::shared_ptr<Texture>* GetHiZPrevious() { return &m_HiZPrevious; }
    const Texture* GetHiZPrevious() const { return m_HiZPrevious.get(); }
    Sampler GetSampler() const { return m_MinMaxSampler; }
    const std::vector<ImageViewHandle>& GetViewHandles() const { return m_MipmapViewHandles; }

    /* NOTE: this is not hiz resolution (which is a power of 2), but the resolution that was passed into constructor */
    const glm::uvec2& GetDrawResolution() const { return m_DrawResolution; }
    const glm::uvec2& GetHiZResolution() const { return m_HiZResolution; }
    
private:
    RG::Resource m_HiZResource{};
    Texture m_HiZ;
    std::shared_ptr<Texture> m_HiZPrevious{};
    Sampler m_MinMaxSampler;
    std::vector<ImageViewHandle> m_MipmapViewHandles;
    
    glm::uvec2 m_DrawResolution{};
    glm::uvec2 m_HiZResolution{};
};

namespace Passes::HiZ
{
    struct PassData
    {
        Sampler MinMaxSampler;
        std::vector<ImageViewHandle> MipmapViewHandles;

        RG::Resource DepthIn{};
        RG::Resource HiZOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth,
        ImageSubresourceDescription::Packed subresource, HiZPassContext& ctx);
}
