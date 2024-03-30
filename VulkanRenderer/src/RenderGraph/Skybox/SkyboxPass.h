#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"

class SkyboxPass
{
public:
    struct PassData
    {
        RenderGraph::Resource Skybox{};
        RenderGraph::Resource DepthIn{};
        RenderGraph::Resource ColorOut{};

        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    SkyboxPass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, const Texture& skybox, RenderGraph::Resource colorOut,
        RenderGraph::Resource depthIn, const glm::uvec2& resolution);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData{};
};
