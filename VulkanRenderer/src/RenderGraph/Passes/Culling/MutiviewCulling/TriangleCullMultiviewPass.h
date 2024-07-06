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
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;

    RG::PipelineData m_PipelineData;
};

struct TriangleCullMultiviewPassInitInfo
{
    const CullMultiviewData* MultiviewData{nullptr};
    CullStage Stage{CullStage::Cull};
};

struct TriangleCullMultiviewPassExecutionInfo
{
    RG::CullTrianglesMultiviewResource* MultiviewResource{nullptr};
};

class TriangleCullMultiviewPass
{
public:
    struct PassData
    {
        std::vector<RG::DrawAttachmentResources> DrawAttachmentResources{};
    };
public:
    TriangleCullMultiviewPass(RG::Graph& renderGraph, std::string_view name,
        const TriangleCullMultiviewPassInitInfo& info);
    void AddToGraph(RG::Graph& renderGraph, const TriangleCullMultiviewPassExecutionInfo& info);
    Utils::StringHasher GetNameHash() const { return m_Name.Hash(); }
private:
    /* this is a more convenient representation */
    struct DrawPipeline
    {
        std::vector<ShaderPipeline> Pipelines;
        std::vector<ShaderDescriptors> ImmutableSamplerDescriptors;
        std::vector<ShaderDescriptors> ResourceDescriptors;
        std::vector<ShaderDescriptors> MaterialDescriptors;
    };
    struct PassDataPrivate
    {
        RG::CullTrianglesMultiviewResource* MultiviewResource{nullptr};
        std::vector<RG::DrawExecutionInfo> TriangleDrawInfos;
        
        std::array<RG::PipelineData, TriangleCullMultiviewTraits::MAX_BATCHES>* CullPipelines{nullptr};
        std::array<RG::PipelineData, TriangleCullMultiviewTraits::MAX_BATCHES>* PreparePipelines{nullptr};
        std::array<DrawPipeline, TriangleCullMultiviewTraits::MAX_BATCHES>*
            DrawPipelines{nullptr};

        std::array<SplitBarrier, TriangleCullMultiviewTraits::MAX_BATCHES>* SplitBarriers{nullptr};
        DependencyInfo* SplitBarrierDependency{nullptr};
    };
private:
    RG::Pass* m_Pass{nullptr};
    RG::PassName m_Name;
    CullStage m_Stage{CullStage::Cull};

    std::array<RG::PipelineData, TriangleCullMultiviewTraits::MAX_BATCHES> m_CullPipelines;
    std::array<RG::PipelineData, TriangleCullMultiviewTraits::MAX_BATCHES> m_PreparePipelines;
    std::array<DrawPipeline, TriangleCullMultiviewTraits::MAX_BATCHES> m_DrawPipelines;

    std::array<SplitBarrier, TriangleCullMultiviewTraits::MAX_BATCHES> m_SplitBarriers{};
    DependencyInfo m_SplitBarrierDependency{};
};
