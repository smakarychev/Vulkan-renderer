#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"

class SkyGradientPass
{
public:
    struct CameraUBO
    {
        glm::mat4 ViewInverse;
        glm::vec3 Position;
    };
    struct SettingsUBO
    {
        glm::vec4 SkyColorHorizon{0.42f, 0.66f, 0.66f, 1.0f};
        glm::vec4 SkyColorZenith{0.12f, 0.32f, 0.54f, 1.0f};
        glm::vec4 GroundColor{0.21f, 0.21f, 0.11f, 1.0f};
        glm::vec4 SunDirection{0.1f, -0.1f, 0.1f, 0.0f};
        f32 SunRadius{128.0f};
        f32 SunIntensity{0.5f};
        f32 GroundToSkyWidth{0.01f};
        f32 HorizonToZenithWidth{0.35f};
        f32 GroundToSkyRate{2.7f};
        f32 HorizonToZenithRate{2.5f};
    };
    struct PushConstants
    {
        glm::uvec2 ImageSize;
    };
    struct PassData
    {
        RG::Resource ColorOut;
        RG::Resource Camera;
        RG::Resource Settings;

        CameraUBO CameraData;

        RG::PipelineData* PipelineData{nullptr};
        
        SettingsUBO* SettingsData{nullptr};
        PushConstants PushConstants;
    };
public:
    SkyGradientPass(RG::Graph& renderGraph);
    void AddToGraph(RG::Graph& renderGraph, RG::Resource renderTarget);
private:
    RG::Pass* m_Pass{nullptr};

    RG::PipelineData m_PipelineData;
    SettingsUBO m_Settings;
};
