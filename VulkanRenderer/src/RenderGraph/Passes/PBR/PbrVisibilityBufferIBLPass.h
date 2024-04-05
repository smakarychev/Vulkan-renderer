#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/RGDrawResources.h"

struct PbrVisibilityBufferInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
};

struct PbrVisibilityBufferExecutionInfo
{
    RenderGraph::Resource VisibilityTexture{};
    RenderGraph::Resource ColorIn{};
    
    RenderGraph::IBLData IBL{};
    RenderGraph::SSAOData SSAO{};

    const RenderPassGeometry* Geometry{nullptr};
};

class PbrVisibilityBufferIBL
{
public:
    struct PassData
    {
        RenderGraph::Resource VisibilityTexture{};
        RenderGraph::IBLData IBL{};
        RenderGraph::SSAOData SSAO{};
        
        RenderGraph::Resource CameraUbo{};
        RenderGraph::Resource CommandsSsbo{};
        RenderGraph::Resource ObjectsSsbo{};
        RenderGraph::Resource PositionsSsbo{};
        RenderGraph::Resource NormalsSsbo{};
        RenderGraph::Resource TangentsSsbo{};
        RenderGraph::Resource UVsSsbo{};
        RenderGraph::Resource IndicesSsbo{};

        RenderGraph::Resource ColorOut{};
        
        RenderGraph::BindlessTexturesPipelineData* PipelineData{nullptr};        
    };
public:
    PbrVisibilityBufferIBL(RenderGraph::Graph& renderGraph, const PbrVisibilityBufferInitInfo& info);
    void AddToGraph(RenderGraph::Graph& renderGraph, const PbrVisibilityBufferExecutionInfo& info);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::BindlessTexturesPipelineData m_PipelineData;
};
