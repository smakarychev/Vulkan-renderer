#pragma once

#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Culling/CullingTraits.h"

class CullMultiviewData;

namespace RG
{
    struct CullMultiviewResources;
}

struct MeshletCullMultiviewPassInitInfo
{
    const CullMultiviewData* MultiviewData{nullptr};
    CullStage Stage{CullStage::Cull};
};

struct MeshletCullMultiviewPassExecutionInfo
{
    RG::CullMultiviewResources* MultiviewResource{nullptr};
};


class MeshletCullMultiviewPass
{
public:
    struct PassData
    {
        RG::CullMultiviewResources* MultiviewResource{nullptr};
        
        RG::PipelineData* PipelineData{nullptr};

        CullStage CullStage{CullStage::Cull};
    };
public:
    MeshletCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
        const MeshletCullMultiviewPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const MeshletCullMultiviewPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;
    CullStage m_Stage{CullStage::Cull};
    
    RG::PipelineData m_PipelineData{};    
};
