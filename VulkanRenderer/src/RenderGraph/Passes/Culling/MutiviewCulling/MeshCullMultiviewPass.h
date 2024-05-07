#pragma once
#include "CullMultiviewResource.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"

struct MeshCullMultiviewPassInitInfo
{
    const CullMultiviewData* MultiviewData{nullptr};
    CullStage Stage{CullStage::Cull};
};

struct MeshCullMultiviewPassExecutionInfo
{
    RG::CullMultiviewResource* MultiviewResource{nullptr};
};

class MeshCullMultiviewPass
{
public:
    struct PassData
    {
        RG::CullMultiviewResource* MultiviewResource{nullptr};
        
        RG::PipelineData* PipelineData{nullptr};
        const CullMultiviewData* MultiviewData{nullptr};
    };
public:
    MeshCullMultiviewPass(RG::Graph& renderGraph, std::string_view name, const MeshCullMultiviewPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, MeshCullMultiviewPassExecutionInfo& info);
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;
    const CullMultiviewData* m_MultiviewData{nullptr};
    CullStage m_Stage{CullStage::Cull};
    
    RG::PipelineData m_PipelineData{};
};
