#pragma once
#include "RenderGraph/RenderPass.h"

class HiZPass
{
    static constexpr u32 MAX_MIPMAP_COUNT = 16;
public:
    struct PassData
    {
        Sampler MinMaxSampler;
        std::array<ImageViewHandle, MAX_MIPMAP_COUNT> MipmapViewHandles;

        RenderGraph::Resource DepthIn;
        RenderGraph::Resource HiZOut;
        RenderGraph::Resource HiZMain;
    };
public:
    HiZPass(RenderGraph::Graph& renderGraph, RenderGraph::Resource depth);
    ~HiZPass();
private:
    void AddSubPasses(RenderGraph::Graph& renderGraph,  RenderGraph::Resource depth);

    void CreateHiZ(const Texture& depth);
private:
    Texture m_HiZ;
    Sampler m_MinMaxSampler;
    std::array<ImageViewHandle, MAX_MIPMAP_COUNT> m_MipmapViewHandles;
    std::array<RenderGraph::Pass*, MAX_MIPMAP_COUNT> m_Passes;
};
