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
    bool SubsequentTriangleCulling{false};
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
        const CullMultiviewData* MultiviewData{nullptr};

        CullStage CullStage{CullStage::Cull};
        bool SubsequentTriangleCulling{false};
    };
public:
    MeshletCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
        const MeshletCullMultiviewPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const MeshletCullMultiviewPassExecutionInfo& info);
    utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;
    const CullMultiviewData* m_MultiviewData{nullptr};
    CullStage m_Stage{CullStage::Cull};
    bool m_SubsequentTriangleCulling{false};
    
    RG::PipelineData m_PipelineData{};    
};
