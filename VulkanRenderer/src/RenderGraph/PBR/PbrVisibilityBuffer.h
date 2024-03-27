#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"

struct PbrVisibilityBufferInitInfo
{
    const ShaderDescriptors* MaterialDescriptors{nullptr};
};

struct PbrVisibilityBufferExecutionInfo
{
    RenderGraph::Resource VisibilityTexture{};
    RenderGraph::Resource ColorIn{};
    
    const RenderPassGeometry* Geometry{nullptr};
};

class PbrVisibilityBuffer
{
public:
    struct CameraUBO
    {
        glm::mat4 View;
        glm::mat4 Projection;
        glm::mat4 ViewProjection;
        glm::mat4 ViewProjectionInverse;
        glm::vec4 CameraPosition;
        glm::vec2 Resolution;
        f32 FrustumNear;
        f32 FrustumFar;
    };
    struct PassData
    {
        RenderGraph::Resource VisibilityTexture{};
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
    PbrVisibilityBuffer(RenderGraph::Graph& renderGraph, const PbrVisibilityBufferInitInfo& info);
    void AddToGraph(RenderGraph::Graph& renderGraph, const PbrVisibilityBufferExecutionInfo& info);
private:
    RenderGraph::Pass* m_Pass{nullptr};

    RenderGraph::BindlessTexturesPipelineData m_PipelineData;
};
