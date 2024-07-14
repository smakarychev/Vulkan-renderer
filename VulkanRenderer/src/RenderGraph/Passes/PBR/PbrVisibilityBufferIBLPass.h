#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGDrawResources.h"

class SceneLight;
class SceneGeometry;

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
    RG::CSMData CSMData{};

    const SceneGeometry* Geometry{nullptr};
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
        RG::CSMData CSMData{};
        
        RG::Resource Camera{};
        RG::Resource ShadingSettings{};
        RG::Resource Commands{};
        RG::Resource Objects{};
        RG::Resource Positions{};
        RG::Resource Normals{};
        RG::Resource Tangents{};
        RG::Resource UVs{};
        RG::Resource Indices{};

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
