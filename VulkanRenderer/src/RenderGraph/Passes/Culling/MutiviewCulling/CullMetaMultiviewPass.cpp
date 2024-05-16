#include "CullMetaMultiviewPass.h"

#include "RenderGraph/RGUtils.h"

CullMetaMultiviewPass::CullMetaMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const CullMetaMultiviewPassInitInfo& info)
        : m_Name(name), m_MultiviewData(info.MultiviewData)
{
    m_HiZs.resize(info.MultiviewData->Views().size());
    m_HiZsReocclusion.resize(info.MultiviewData->Views().size());
    for (u32 i = 0; i < m_HiZs.size(); i++)
    {
        m_HiZs[i] = std::make_unique<HiZPass>(renderGraph, std::format("{}.HiZ.{}", name, i));
        m_HiZsReocclusion[i] = std::make_unique<HiZPass>(renderGraph, std::format("{}.HiZ.Reocclusion.{}", name, i));
    }

    m_MeshCull = std::make_unique<MeshCullMultiviewPass>(renderGraph, std::format("{}.MeshCull", name),
        MeshCullMultiviewPassInitInfo{
            .MultiviewData = m_MultiviewData,
            .Stage = CullStage::Cull});
    m_MeshReocclusion = std::make_unique<MeshCullMultiviewPass>(renderGraph, std::format("{}.MeshReocclusion", name),
        MeshCullMultiviewPassInitInfo{
            .MultiviewData = m_MultiviewData,
            .Stage = CullStage::Reocclusion});
    
    m_MeshletCull = std::make_unique<MeshletCullMultiviewPass>(renderGraph, std::format("{}.MeshletCull", name),
        MeshletCullMultiviewPassInitInfo{
            .MultiviewData = m_MultiviewData,
            .Stage = CullStage::Cull,
            // todo: fix me once triangle culling is done
            .SubsequentTriangleCulling = false});
    m_MeshletReocclusion = std::make_unique<MeshletCullMultiviewPass>(renderGraph,
        std::format("{}.MeshletReocclusion", name),
        MeshletCullMultiviewPassInitInfo{
            .MultiviewData = m_MultiviewData,
            .Stage = CullStage::Reocclusion,
            .SubsequentTriangleCulling = false});

    m_Draws.resize(info.MultiviewData->Views().size());
    m_DrawsReocclusion.resize(info.MultiviewData->Views().size());
    for (u32 i = 0; i < m_Draws.size(); i++)
    {
        DrawIndirectCountPassInitInfo drawInfo = {
            .DrawFeatures = info.MultiviewData->Views()[i].Static.DrawFeatures,
            .DrawPipeline = *info.MultiviewData->Views()[i].Static.DrawMeshletsPipeline,
            .MaterialDescriptors = info.MultiviewData->Views()[i].Static.MaterialDescriptors};

        m_Draws[i] = std::make_unique<DrawIndirectCountPass>(renderGraph,
            std::format("{}.Draw.Meshlet.{}", name, i),
            drawInfo);
        
        m_DrawsReocclusion[i] = std::make_unique<DrawIndirectCountPass>(renderGraph,
            std::format("{}.Draw.Meshlet.Reocclusion.{}", name, i),
            drawInfo);
    }
}

void CullMetaMultiviewPass::AddToGraph(RG::Graph& renderGraph)
{
    using namespace RG;

    m_AttachmentRenames.clear();

    for (auto& view : m_MultiviewData->Views())
    {
        if (!view.Static.HiZContext ||
            view.Static.HiZContext->GetDrawResolution().x != view.Dynamic.Resolution.x ||
            view.Static.HiZContext->GetDrawResolution().y != view.Dynamic.Resolution.y ||
            renderGraph.ChangedResolution())
        {
            view.Static.HiZContext = std::make_shared<HiZPassContext>(view.Dynamic.Resolution,
                renderGraph.GetResolutionDeletionQueue());
        }
    }

    m_MultiviewResource = RgUtils::createCullMultiview(*m_MultiviewData, renderGraph,
        m_Name.Name());

    m_MeshCull->AddToGraph(renderGraph, MeshCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    m_MeshletCull->AddToGraph(renderGraph, MeshletCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    auto& meshletCullOutput = renderGraph.GetBlackboard().Get<MeshletCull::PassData>(m_MeshletCull->GetNameHash());

    /* ensure all views have valid attachments */
    for (auto& view : m_MultiviewData->Views())
        EnsureViewAttachments(renderGraph, view.Dynamic);
    
    for (u32 i = 0; i < m_Draws.size(); i++)
    {
        auto& view = m_MultiviewData->Views()[i];
        UpdateRecordedAttachmentResources(view.Dynamic.DrawAttachments);
        m_Draws[i]->AddToGraph(renderGraph, {
            .Geometry = view.Static.Geometry,
            .Commands = meshletCullOutput.MultiviewResource->CompactCommands[i],
            .CommandCount = meshletCullOutput.MultiviewResource->CompactCommandCount[i],
            .Resolution = view.Dynamic.Resolution,
            .Camera = view.Dynamic.Camera,
            .DrawAttachments = view.Dynamic.DrawAttachments,
           // .SceneLights = *view.Dynamic.SceneLights, // todo: fix me
            .IBL = view.Dynamic.IBL,
            .SSAO = view.Dynamic.SSAO});
        
        auto& drawOutput = renderGraph.GetBlackboard().Get<DrawIndirectCountPass::PassData>(m_Draws[i]->GetNameHash());
        RecordUpdatedAttachmentResources(view.Dynamic.DrawAttachments, drawOutput.DrawAttachmentResources);
    }
    for (u32 i = 0; i < m_Draws.size(); i++)
    {
        auto& view = m_MultiviewData->Views()[i];
        if (view.Dynamic.DrawAttachments.Depth.has_value())
            m_HiZs[i]->AddToGraph(renderGraph, view.Dynamic.DrawAttachments.Depth->Resource,
                view.Dynamic.DrawAttachments.Depth->Description.Subresource, *view.Static.HiZContext);
    }
    
    m_MeshReocclusion->AddToGraph(renderGraph, MeshCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    m_MeshletReocclusion->AddToGraph(renderGraph, MeshletCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    auto& meshletReocclusionOutput = renderGraph.GetBlackboard().Get<MeshletCull::PassData>(
        m_MeshletReocclusion->GetNameHash());

    for (u32 i = 0; i < m_Draws.size(); i++)
    {
        auto& view = m_MultiviewData->Views()[i];
        UpdateRecordedAttachmentResources(view.Dynamic.DrawAttachments);
        SetAttachmentsLoadOperation(AttachmentLoad::Load, view.Dynamic);
        m_DrawsReocclusion[i]->AddToGraph(renderGraph, {
            .Geometry = view.Static.Geometry,
            .Commands = meshletReocclusionOutput.MultiviewResource->CompactCommands[i],
            .CommandCount = meshletReocclusionOutput.MultiviewResource->CompactCommandCountReocclusion[i],
            .Resolution = view.Dynamic.Resolution,
            .Camera = view.Dynamic.Camera,
            .DrawAttachments = view.Dynamic.DrawAttachments,
           // .SceneLights = *view.Dynamic.SceneLights, // todo: fix me
            .IBL = view.Dynamic.IBL,
            .SSAO = view.Dynamic.SSAO});
        
        auto& drawOutput = renderGraph.GetBlackboard().Get<DrawIndirectCountPass::PassData>(
            m_DrawsReocclusion[i]->GetNameHash());
        RecordUpdatedAttachmentResources(view.Dynamic.DrawAttachments, drawOutput.DrawAttachmentResources);
    }

    PassData passData = {};
    passData.DrawAttachmentResources.resize(m_Draws.size());
    passData.HiZOut.resize(m_Draws.size());
    for (u32 i = 0; i < m_DrawsReocclusion.size(); i++)
    {
        auto& drawOutput = renderGraph.GetBlackboard().Get<DrawIndirectCountPass::PassData>(
            m_Draws[i]->GetNameHash());
        // todo: fix me once triangle culling is done (hiz will be in m_HiZsReocclusion)
        auto& hizOutput = renderGraph.GetBlackboard().Get<HiZPass::PassData>(m_HiZs[i]->GetNameHash());
        
        passData.DrawAttachmentResources[i] = drawOutput.DrawAttachmentResources;
        passData.HiZOut[i] = hizOutput.HiZOut;
    }

    renderGraph.GetBlackboard().Register(m_Name.Hash(), passData);
}

void CullMetaMultiviewPass::EnsureViewAttachments(RG::Graph& renderGraph, CullViewDynamicDescription& view)
{
    for (u32 i = 0; i < view.DrawAttachments.Colors.size(); i++)
    {
        auto& color = view.DrawAttachments.Colors[i];
        color.Resource = RG::RgUtils::ensureResource(color.Resource, renderGraph,
            std::format("{}.ColorIn.{}", m_Name.Name(), i),
            RG::GraphTextureDescription{
                    .Width = view.Resolution.x,
                    .Height = view.Resolution.y,
                    .Format =  Format::RGBA16_FLOAT});
    }
    
    if (view.DrawAttachments.Depth.has_value())
    {
        auto& depth = *view.DrawAttachments.Depth;
        depth.Resource = RG::RgUtils::ensureResource(depth.Resource, renderGraph,
            std::format("{}.Depth", m_Name.Name()),
            RG::GraphTextureDescription{
                .Width = view.Resolution.x,
                .Height = view.Resolution.y,
                .Format =  Format::D32_FLOAT}); 
    }
}

void CullMetaMultiviewPass::SetAttachmentsLoadOperation(AttachmentLoad load, CullViewDynamicDescription& view)
{
    for (u32 i = 0; i < view.DrawAttachments.Colors.size(); i++)
    {
        auto& color = view.DrawAttachments.Colors[i];
        color.Description.OnLoad = load;
    }
    
    if (view.DrawAttachments.Depth.has_value())
    {
        auto& depth = *view.DrawAttachments.Depth;
        depth.Description.OnLoad = load; 
    }
}

void CullMetaMultiviewPass::RecordUpdatedAttachmentResources(const RG::DrawAttachments& old,
    const RG::DrawAttachmentResources& updated)
{
    for (u32 i = 0; i < old.Colors.size(); i++)
        if (old.Colors[i].Resource != updated.Colors[i])
            m_AttachmentRenames[old.Colors[i].Resource] = updated.Colors[i];
    if (old.Depth.has_value())
        if (old.Depth->Resource != *updated.Depth)
            m_AttachmentRenames[old.Depth->Resource] = *updated.Depth;
}

void CullMetaMultiviewPass::UpdateRecordedAttachmentResources(RG::DrawAttachments& attachments)
{
    for (auto& color : attachments.Colors)
        while (m_AttachmentRenames.contains(color.Resource))
            color.Resource = m_AttachmentRenames.at(color.Resource);
    if (attachments.Depth.has_value())
        while (m_AttachmentRenames.contains(attachments.Depth->Resource))
            attachments.Depth->Resource = m_AttachmentRenames.at(attachments.Depth->Resource);
}
