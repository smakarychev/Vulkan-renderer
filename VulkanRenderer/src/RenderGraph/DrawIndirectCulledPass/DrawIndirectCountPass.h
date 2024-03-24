#pragma once

#include <glm/glm.hpp>

#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassCommon.h"

struct DrawIndirectCountPassInitInfo
{
    RenderGraph::Resource Color;
    RenderGraph::Resource Depth;
    RenderGraph::Resource Commands;
    RenderGraph::Resource CommandCount;
    glm::uvec2 Resolution;
    bool ClearDepth{false};
};

class DrawIndirectCountPass
{
public:
    struct CameraUBO
    {
        glm::mat4 ViewProjection;
    };
    struct PassData
    {
        RenderGraph::Resource CameraUbo;
        RenderGraph::Resource ObjectsSsbo;
        RenderGraph::Resource CommandsIndirect;
        RenderGraph::Resource CountIndirect;
        RenderGraph::Resource ColorOut;
        RenderGraph::Resource DepthOut;

        RenderGraph::PipelineData* PipelineData{nullptr};
    };
public:
    DrawIndirectCountPass(RenderGraph::Graph& renderGraph, std::string_view name);
    void AddToGraph(RenderGraph::Graph& renderGraph, const RenderPassGeometry& geometry,
        const DrawIndirectCountPassInitInfo& initInfo);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RenderGraph::Pass* m_Pass{nullptr};
    RenderGraph::PassName m_Name;
    
    RenderGraph::PipelineData m_PipelineData;
};