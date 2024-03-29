#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"

class SsaoPass
{
    static constexpr u32 MAX_SAMPLES_COUNT{256};
public:
    struct SettingsUBO
    {
        f32 Power{1.0f};
        f32 Radius{0.5f};
        u32 Samples{0};
    };
    struct CameraUBO
    {
        glm::mat4 Projection{glm::mat4{1.0f}};
        glm::mat4 ProjectionInverse{glm::mat4{1.0f}};
        f32 Near{0.0f};
        f32 Far{1000.0f};
    };
    struct PassData
    {
        RenderGraph::Resource DepthIn{};
        RenderGraph::Resource SSAO{};

        RenderGraph::Resource NoiseTexture{};
        RenderGraph::Resource SettingsUbo{};
        RenderGraph::Resource CameraUbo{};
        RenderGraph::Resource SamplesUbo{};

        RenderGraph::PipelineData* PipelineData{nullptr};

        SettingsUBO* Settings{nullptr};
        u32 SampleCount{0};
    };
public:
    SsaoPass(RenderGraph::Graph& renderGraph, u32 sampleCount);
    void AddToGraph(RenderGraph::Graph& renderGraph, RenderGraph::Resource depthIn);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    Texture m_NoiseTexture{};
    Buffer m_SamplesBuffer{};
    u32 m_SampleCount{0};
    
    RenderGraph::PipelineData m_PipelineData{};

    SettingsUBO m_Settings{};
};
