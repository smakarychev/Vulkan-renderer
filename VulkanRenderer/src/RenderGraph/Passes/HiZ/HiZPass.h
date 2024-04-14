#pragma once
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"

class HiZPassContext
{
public:
    static constexpr u32 MAX_MIPMAP_COUNT = 16;
    HiZPassContext(const glm::uvec2& resolution, DeletionQueue& deletionQueue);

    const Texture& GetHiZ() const { return m_HiZ; }
    std::shared_ptr<Texture>* GetHiZPrevious() { return &m_HiZPrevious; }
    const Texture* GetHiZPrevious() const { return m_HiZPrevious.get(); }
    Sampler GetSampler() const { return m_MinMaxSampler; }
    const std::vector<ImageViewHandle>& GetViewHandles() const { return m_MipmapViewHandles; }

    /* NOTE: this is not hiz resolution (which is a power of 2), but the resolution that was passed into constructor */
    const glm::uvec2& GetDrawResolution() const { return m_DrawResolution; }
    
private:
    Texture m_HiZ;
    std::shared_ptr<Texture> m_HiZPrevious{};
    Sampler m_MinMaxSampler;
    std::vector<ImageViewHandle> m_MipmapViewHandles;
    
    glm::uvec2 m_DrawResolution{};
};

class HiZPass
{
public:
    struct PassData
    {
        Sampler MinMaxSampler;
        std::vector<ImageViewHandle> MipmapViewHandles;

        RG::Resource DepthIn{};
        RG::Resource HiZOut{};
        
        RG::PipelineData* PipelineData{nullptr};
    };
public:
    HiZPass(RG::Graph& renderGraph, std::string_view baseName);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource depth, HiZPassContext& ctx);
private:
    std::array<RG::Pass*, HiZPassContext::MAX_MIPMAP_COUNT> m_Passes{};
    RG::PassName m_Name;

    std::array<RG::PipelineData, HiZPassContext::MAX_MIPMAP_COUNT> m_PipelinesData;
};
