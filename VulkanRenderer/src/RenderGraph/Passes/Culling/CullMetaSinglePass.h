#pragma once

#include "TriangleCullDrawPass.h"
#include "RenderGraph/RenderGraph.h"

struct CullMetaPassExecutionInfo;

struct CullMetaSinglePassInitInfo
{
    using Features = RG::DrawFeatures;
    const SceneGeometry* Geometry{nullptr};
    const ShaderPipeline* DrawPipeline{nullptr};
    const ShaderPipeline* DrawMeshletsPipeline{nullptr};
    const ShaderDescriptors* MaterialDescriptors{nullptr};
    Features DrawFeatures{Features::AllAttributes};
    bool ClampDepth{false};
    CameraType CameraType{CameraType::Perspective};
};

class CullMetaSinglePass
{
public:
    struct PassData
    {
        RG::DrawAttachmentResources DrawAttachmentResources{};
    };
public:
    CullMetaSinglePass(RG::Graph& renderGraph, const CullMetaSinglePassInitInfo& info, std::string_view name);
    void AddToGraph(RG::Graph& renderGraph, const CullMetaPassExecutionInfo& info,
        HiZPassContext& hiZContext);
    u64 GetNameHash() const { return m_Name.Hash(); }
    const std::string& GetName() const { return m_Name.Name(); }
private:
    RG::PassName m_Name;
    PassData m_PassData;

    RG::DrawFeatures m_DrawFeatures{RG::DrawFeatures::AllAttributes};

    using MeshCull = MeshCullSinglePass;
    using MeshletCull = MeshletCullSinglePass;

    std::shared_ptr<MeshCullContext> m_MeshContext;
    std::shared_ptr<MeshCull> m_MeshCull;
    
    std::shared_ptr<MeshletCullContext> m_MeshletContext;
    std::shared_ptr<MeshletCull> m_MeshletCull;

    std::shared_ptr<TriangleCullContext> m_TriangleContext;
    std::shared_ptr<TriangleDrawContext> m_TriangleDrawContext;
    std::shared_ptr<TriangleCullPrepareDispatchPass> m_TrianglePrepareDispatch; 

    using TriangleCullDraw = TriangleCullDrawPass<CullStage::Single>;
    
    std::shared_ptr<TriangleCullDraw> m_CullDraw;
};
