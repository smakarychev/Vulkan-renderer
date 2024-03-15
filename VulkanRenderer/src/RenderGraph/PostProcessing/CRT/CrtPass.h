#pragma once

#include "RenderGraph/RenderGraphResource.h"
#include "RenderGraph/RenderPassCommon.h"

class CrtPass
{
public:
    struct SettingsUBO
    {
        f32 Curvature{0.2f};
        f32 ColorSplit{0.004f};
        f32 LinesMultiplier{1.0f};
        f32 VignettePower{0.64f};
        f32 VignetteRadius{0.025f};
    };
    struct PassData
    {
        RenderGraph::Resource ColorIn;
        RenderGraph::Resource ColorTarget{};
        RenderGraph::Resource TimeUbo{};
        RenderGraph::Resource SettingsUbo{};

        RenderGraph::PipelineData* PipelineData{nullptr};
        
        SettingsUBO* Settings{nullptr};
    };
public:
    CrtPass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource colorIn, RenderGraph::Resource colorTarget);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::PipelineData m_PipelineData;
    SettingsUBO m_SettingsUBO{};
};
