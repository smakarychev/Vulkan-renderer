#pragma once
#include "CullMultiviewResources.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"

class TriangleCullPrepareMultiviewPass
{
public:
    struct PassData
    {
        RG::CullTrianglesMultiviewResource* MultiviewResource{nullptr};
        
        RG::PipelineData* PipelineData{nullptr};
    };
public:
    TriangleCullPrepareMultiviewPass(RG::Graph& renderGraph, std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, RG::CullTrianglesMultiviewResource* multiviewResource);
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData;
};

class TriangleCullMultiviewPass
{
public:
    
};
