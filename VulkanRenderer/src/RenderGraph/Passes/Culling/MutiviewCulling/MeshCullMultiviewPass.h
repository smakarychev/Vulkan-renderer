#pragma once
#include "CullMultiviewResources.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"

struct MeshCullMultiviewPassInitInfo
{
    const CullMultiviewData* MultiviewData{nullptr};
    CullStage Stage{CullStage::Cull};
};

struct MeshCullMultiviewPassExecutionInfo
{
    RG::CullMultiviewResources* MultiviewResource{nullptr};
};

class MeshCullMultiviewPass
{
public:
    struct PassData
    {
        RG::CullMultiviewResources* MultiviewResource{nullptr};
        
        RG::PipelineData* PipelineData{nullptr};
    };
public:
    MeshCullMultiviewPass(RG::Graph& renderGraph, std::string_view name, const MeshCullMultiviewPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const MeshCullMultiviewPassExecutionInfo& info);
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;
    CullStage m_Stage{CullStage::Cull};
    
    RG::PipelineData m_PipelineData{};
};
