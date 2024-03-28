#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"

class SsaoPSPass
{
public:
    struct SettingsUBO
    {
        f32 TotalStrength{1.0f};
        f32 Base{0.2f};
        f32 Area{0.0075f};
        f32 Falloff{0.000001f};
        f32 Radius{0.0002f};
    };
    struct CameraUBO
    {
        glm::mat4 ProjectionInverse{glm::mat4{1.0f}};
    };
    struct PassData
    {
        RenderGraph::Resource DepthIn{};
        RenderGraph::Resource SSAO{};

        RenderGraph::Resource NoiseTexture{};
        RenderGraph::Resource SettingsUbo{};
        RenderGraph::Resource CameraUbo{};
        

        RenderGraph::PipelineData* PipelineData{nullptr};

        SettingsUBO* Settings{nullptr};
    };
public:
    SsaoPSPass(RenderGraph::Graph& renderGraph);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource depthIn);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    Texture m_NoiseTexture{};

    RenderGraph::PipelineData m_PipelineData{};

    SettingsUBO m_Settings{};
};
