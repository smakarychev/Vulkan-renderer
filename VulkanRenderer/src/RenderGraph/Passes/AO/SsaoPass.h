#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

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
        RG::Resource DepthIn{};
        RG::Resource SSAO{};

        RG::Resource NoiseTexture{};
        RG::Resource SettingsUbo{};
        RG::Resource CameraUbo{};
        RG::Resource SamplesUbo{};

        RG::PipelineData* PipelineData{nullptr};

        SettingsUBO* Settings{nullptr};
        u32 SampleCount{0};
    };
public:
    SsaoPass(RG::Graph& renderGraph, u32 sampleCount);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource depthIn);
private:
    RG::Pass* m_Pass{nullptr};

    Texture m_NoiseTexture{};
    Buffer m_SamplesBuffer{};
    u32 m_SampleCount{0};
    
    RG::PipelineData m_PipelineData{};

    SettingsUBO m_Settings{};
};
