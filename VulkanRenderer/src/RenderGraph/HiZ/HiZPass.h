#pragma once
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RenderPassCommon.h"

class HiZPassContext
{
public:
    static constexpr u32 MAX_MIPMAP_COUNT = 16;
    HiZPassContext(const glm::uvec2& resolution);
    ~HiZPassContext();

    const Texture& GetHiZ() const { return m_HiZ; }
    std::shared_ptr<Texture>* GetHiZPrevious() { return &m_HiZPrevious; }
    const Texture* GetHiZPrevious() const { return m_HiZPrevious.get(); }
    Sampler GetSampler() const { return m_MinMaxSampler; }
    const std::array<ImageViewHandle, MAX_MIPMAP_COUNT>& GetViewHandles() const { return m_MipmapViewHandles; }
private:
    Texture m_HiZ;
    std::shared_ptr<Texture> m_HiZPrevious{};
    Sampler m_MinMaxSampler;
    std::array<ImageViewHandle, MAX_MIPMAP_COUNT> m_MipmapViewHandles;
};

class HiZPass
{
public:
    struct PassData
    {
        Sampler MinMaxSampler;
        std::array<ImageViewHandle, HiZPassContext::MAX_MIPMAP_COUNT> MipmapViewHandles;

        RenderGraph::Resource DepthIn;
        RenderGraph::Resource HiZOut;
        
        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    HiZPass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource depth, HiZPassContext& ctx);
private:
    std::array<RenderGraph::Pass*, HiZPassContext::MAX_MIPMAP_COUNT> m_Passes{};

    std::array<RenderGraph::PipelineData, HiZPassContext::MAX_MIPMAP_COUNT> m_PipelinesData;
};