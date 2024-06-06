#pragma once
#include "CullMultiviewResources.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"

struct TriangleCullPrepareMultiviewPassExecutionInfo
{
    RG::CullTrianglesMultiviewResource* MultiviewResource{nullptr};
};

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
    void AddToGraph(RG::Graph& renderGraph, const TriangleCullPrepareMultiviewPassExecutionInfo& info);
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData;
};

struct TriangleCullMultiviewPasInitInfo
{
    
};

struct TriangleCullMultiviewPassExecutionInfo
{
    
};

class TriangleCullMultiviewPass
{
public:
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    std::array<RG::PipelineData, TriangleCullMultiviewTraits::MAX_BATCHES> m_CullPipelines;
    std::array<RG::PipelineData, TriangleCullMultiviewTraits::MAX_BATCHES> m_PreparePipelines;
    std::array<RG::BindlessTexturesPipelineData, TriangleCullMultiviewTraits::MAX_BATCHES> m_DrawPipelines;

    RG::DrawFeatures m_Features{RG::DrawFeatures::AllAttributes};
    
    std::array<SplitBarrier, TriangleCullMultiviewTraits::MAX_BATCHES> m_SplitBarriers{};
    DependencyInfo m_SplitBarrierDependency{};
};
