#pragma once
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RenderPassGeometry.h"

class TestPass3d
{
public:
    struct PassData
    {
        RenderGraph::Resource CameraUbo{};
        
        RenderGraph::Resource ColorOut{};
        RenderGraph::Resource DepthOut{};
    };
public:
    TestPass3d(RenderGraph::Graph& renderGraph, const Texture& depthTarget, 
        ResourceUploader* resourceUploader);
private:
    void CreatePass(RenderGraph::Graph& renderGraph, const Texture& depthTarget,
        RenderPassGeometry* geometry);
private:
    RenderGraph::Pass* m_Pass{nullptr};
    ModelCollection m_Collection;
    RenderPassGeometry m_Geometry;
};
