#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGDrawResources.h"

class SceneLight;

namespace RG
{
    class Geometry;
}

struct PbrVisibilityBufferInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
};

struct PbrVisibilityBufferExecutionInfo
{
    RG::Resource VisibilityTexture{};
    RG::Resource ColorIn{};

    const SceneLight* SceneLights{nullptr};
    RG::IBLData IBL{};
    RG::SSAOData SSAO{};
    RG::DirectionalShadowData DirectionalShadowData{};

    const RG::Geometry* Geometry{nullptr};
};

class PbrVisibilityBufferIBL
{
public:
    struct PassData
    {
        RG::Resource VisibilityTexture{};
        RG::SceneLightResources LightsResources{};
        RG::IBLData IBL{};
        RG::SSAOData SSAO{};
        RG::DirectionalShadowData DirectionalShadowData{};
        
        RG::Resource CameraUbo{};
        RG::Resource CommandsSsbo{};
        RG::Resource ObjectsSsbo{};
        RG::Resource PositionsSsbo{};
        RG::Resource NormalsSsbo{};
        RG::Resource TangentsSsbo{};
        RG::Resource UVsSsbo{};
        RG::Resource IndicesSsbo{};

        RG::Resource ColorOut{};
        
        RG::BindlessTexturesPipelineData* PipelineData{nullptr};        
    };
public:
    PbrVisibilityBufferIBL(RG::Graph& renderGraph, const PbrVisibilityBufferInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const PbrVisibilityBufferExecutionInfo& info);
private:
    RG::Pass* m_Pass{nullptr};

    RG::BindlessTexturesPipelineData m_PipelineData;
};
