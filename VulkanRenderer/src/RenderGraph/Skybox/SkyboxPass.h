#pragma once
#include "RenderGraph/RenderGraph.h"
#include "..\RGCommon.h"

class SkyboxPass
{
public:
    struct ProjectionUBO
    {
        glm::mat4 ProjectionInverse{1.0f};
        glm::mat4 ViewInverse{1.0f};
    };
    struct PassData
    {
        RenderGraph::Resource Skybox{};
        RenderGraph::Resource DepthOut{};
        RenderGraph::Resource ColorOut{};
        RenderGraph::Resource ProjectionUbo{};

        f32 LodBias{0.0f};

        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    SkyboxPass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, const Texture& skybox, RenderGraph::Resource colorOut,
        RenderGraph::Resource depthIn, const glm::uvec2& resolution, f32 lodBias);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource skybox, RenderGraph::Resource colorOut,
        RenderGraph::Resource depthIn, const glm::uvec2& resolution, f32 lodBias);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData{};
};
