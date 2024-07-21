#include "CullMetaMultiviewPass.h"

#include "CullMultiviewUtils.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Utility/ImGuiTexturePass.h"

CullMetaMultiviewPass::CullMetaMultiviewPass(RG::Graph& renderGraph, std::string_view name,
    const CullMetaMultiviewPassInitInfo& info)
        : m_Name(name), m_MultiviewData(info.MultiviewData)
{
    m_MeshletOnlyViewIndices.reserve(info.MultiviewData->ViewCount());
    for (u32 i = 0; i < m_MultiviewData->ViewCount(); i++)
    {
        auto& view = m_MultiviewData->View(i);
        if (!view.Static.CullTriangles)
            m_MeshletOnlyViewIndices.push_back((u32)i);
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
            .Stage = CullStage::Cull});
    m_MeshletReocclusion = std::make_unique<MeshletCullMultiviewPass>(renderGraph,
        std::format("{}.MeshletReocclusion", name),
        MeshletCullMultiviewPassInitInfo{
            .MultiviewData = m_MultiviewData,
            .Stage = CullStage::Reocclusion});

    m_TrianglePrepare = std::make_unique<TrianglePrepare>(renderGraph, std::format("{}.TrianglePrepare", name));
    m_TrianglePrepareReocclusion = std::make_unique<TrianglePrepare>(renderGraph,
        std::format("{}.TrianglePrepareReocclusion", name));

    m_TriangleCullDraw = std::make_unique<TriangleCull>(renderGraph, std::format("{}.TriangleCullDraw", name),
        TriangleCullMultiviewPassInitInfo{
            .MultiviewData = m_MultiviewData,
            .Stage = CullStage::Cull});
    m_TriangleReocclusionDraw = std::make_unique<TriangleCull>(renderGraph,
        std::format("{}.TriangleReocclusionDraw", name),
        TriangleCullMultiviewPassInitInfo{
            .MultiviewData = m_MultiviewData,
            .Stage = CullStage::Reocclusion});

    m_MeshletOnlyDraws.resize(m_MeshletOnlyViewIndices.size());
    for (u32 i = 0; i < m_MeshletOnlyDraws.size(); i++)
    {
        u32 viewIndex = m_MeshletOnlyViewIndices[i];
        DrawIndirectCountPassInitInfo drawInfo = {
            .DrawFeatures = info.MultiviewData->View(viewIndex).Static.DrawFeatures,
            .DrawPipeline = *info.MultiviewData->View(viewIndex).Static.DrawMeshletsPipeline,
            .MaterialDescriptors = info.MultiviewData->View(viewIndex).Static.MaterialDescriptors};

        m_MeshletOnlyDraws[i] = std::make_unique<DrawIndirectCountPass>(renderGraph,
            std::format("{}.Draw.Meshlet.{}", name, i),
            drawInfo);
    }
    
    m_DrawsReocclusion.resize(info.MultiviewData->ViewCount());
    for (u32 i = 0; i < m_DrawsReocclusion.size(); i++)
    {
        DrawIndirectCountPassInitInfo drawInfo = {
            .DrawFeatures = info.MultiviewData->View(i).Static.DrawFeatures,
            .DrawPipeline = *info.MultiviewData->View(i).Static.DrawMeshletsPipeline,
            .MaterialDescriptors = info.MultiviewData->View(i).Static.MaterialDescriptors};

        m_DrawsReocclusion[i] = std::make_unique<DrawIndirectCountPass>(renderGraph,
            std::format("{}.Draw.Meshlet.Reocclusion.{}", name, i),
            drawInfo);
    }
}

void CullMetaMultiviewPass::AddToGraph(RG::Graph& renderGraph)
{
    using namespace RG;

    std::unordered_map<Resource, Resource> attachmentRenames;

    for (u32 i = 0; i < m_MultiviewData->ViewCount(); i++)
    {
        auto& view = m_MultiviewData->View(i);
        if (!view.Static.HiZContext ||
            view.Static.HiZContext->GetDrawResolution().x != view.Dynamic.Resolution.x ||
            view.Static.HiZContext->GetDrawResolution().y != view.Dynamic.Resolution.y ||
            renderGraph.ChangedResolution())
        {
            m_MultiviewData->UpdateViewHiZ((u32)i,
                std::make_shared<HiZPassContext>(view.Dynamic.Resolution, renderGraph.GetResolutionDeletionQueue()));
        }
    }

    m_MultiviewResource = RgUtils::createCullMultiview(*m_MultiviewData, renderGraph,
        m_Name.Name());
    m_MultiviewTrianglesResource = RgUtils::createTriangleCullMultiview(m_MultiviewResource,
        renderGraph, m_Name.Name());
    m_MultiviewTrianglesResource.AttachmentsRenames = &attachmentRenames;

    m_MeshCull->AddToGraph(renderGraph, MeshCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    m_MeshletCull->AddToGraph(renderGraph, MeshletCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    auto& meshletCullOutput = renderGraph.GetBlackboard().Get<MeshletCull::PassData>(m_MeshletCull->GetNameHash());

    /* ensure all views have valid attachments */
    for (u32 i = 0; i < m_MultiviewData->ViewCount(); i++)
        EnsureViewAttachments(renderGraph, m_MultiviewData->View(i).Dynamic);

    /* draw views that do not use triangle culling */
    for (u32 i = 0; i < m_MeshletOnlyViewIndices.size(); i++)
    {
        u32 viewIndex = m_MeshletOnlyViewIndices[i];
        auto& view = m_MultiviewData->View(viewIndex);
        Utils::updateRecordedAttachmentResources(view.Dynamic.DrawInfo.Attachments, attachmentRenames);
        m_MeshletOnlyDraws[i]->AddToGraph(renderGraph, {
            .Geometry = view.Static.Geometry,
            .Commands = meshletCullOutput.MultiviewResource->CompactCommands[i],
            .CommandCount = meshletCullOutput.MultiviewResource->CompactCommandCount,
            .CountOffset = i,
            .Resolution = view.Dynamic.Resolution,
            .Camera = view.Dynamic.Camera,
            .DrawInfo = {
                .Attachments = view.Dynamic.DrawInfo.Attachments,
               // .SceneLights = *view.Dynamic.SceneLights, // todo: fix me
                .IBL = view.Dynamic.DrawInfo.IBL,
                .SSAO = view.Dynamic.DrawInfo.SSAO}});
        
        auto& drawOutput = renderGraph.GetBlackboard().Get<DrawIndirectCountPass::PassData>(
            m_MeshletOnlyDraws[i]->GetNameHash());
        Utils::recordUpdatedAttachmentResources(view.Dynamic.DrawInfo.Attachments, drawOutput.DrawAttachmentResources,
            attachmentRenames);
    }

    /* cull and draw views that do use triangle culling */
    if (m_MultiviewTrianglesResource.TriangleViewCount > 0)
    {
        m_TrianglePrepare->AddToGraph(renderGraph, TriangleCullPrepareMultiviewPassExecutionInfo{
            .MultiviewResource = &m_MultiviewTrianglesResource});
        m_TriangleCullDraw->AddToGraph(renderGraph, TriangleCullMultiviewPassExecutionInfo{
            .MultiviewResource = &m_MultiviewTrianglesResource});
    }

    /* update HiZs, now that all previously visible stuff was drawn */
    for (u32 i = 0; i < m_DrawsReocclusion.size(); i++)
    {
        auto& view = m_MultiviewData->View(i);
        if (view.Dynamic.DrawInfo.Attachments.Depth.has_value())
            Passes::HiZ::addToGraph(std::format("{}.HiZ.{}", GetName(), i), renderGraph,
                view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource, *view.Static.HiZContext);
    }

    /* update attachment on load operation */
    for (u32 i = 0; i < m_DrawsReocclusion.size(); i++)
    {
        SetAttachmentsLoadOperation(AttachmentLoad::Load, m_MultiviewData->View(i).Dynamic);

        if (i < m_MultiviewData->TriangleViewCount())
            SetAttachmentsLoadOperation(AttachmentLoad::Load, m_MultiviewData->TriangleView(i).Dynamic);
    }

    /* now we have to do triangle reocclusion */
    if (m_MultiviewTrianglesResource.TriangleViewCount > 0)
    {
        m_TrianglePrepareReocclusion->AddToGraph(renderGraph, TriangleCullPrepareMultiviewPassExecutionInfo{
            .MultiviewResource = &m_MultiviewTrianglesResource});
        m_TriangleReocclusionDraw->AddToGraph(renderGraph, TriangleCullMultiviewPassExecutionInfo{
            .MultiviewResource = &m_MultiviewTrianglesResource});
    }

    /* update HiZs with reoccluded triangles */
    for (u32 i = 0; i < m_MultiviewTrianglesResource.TriangleViewCount; i++)
    {
        auto& view = m_MultiviewData->TriangleView(i);
        if (view.Dynamic.DrawInfo.Attachments.Depth.has_value())
            Passes::HiZ::addToGraph(std::format("{}.HiZ.Reocclusion.{}", GetName(), i), renderGraph,
                view.Dynamic.DrawInfo.Attachments.Depth->Resource,
                view.Dynamic.DrawInfo.Attachments.Depth->Description.Subresource, *view.Static.HiZContext);
    }

    /* finally, reocclude and draw meshlets for each view */
    m_MeshReocclusion->AddToGraph(renderGraph, MeshCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    m_MeshletReocclusion->AddToGraph(renderGraph, MeshletCullMultiviewPassExecutionInfo{
        .MultiviewResource = &m_MultiviewResource});
    auto& meshletReocclusionOutput = renderGraph.GetBlackboard().Get<MeshletCull::PassData>(
        m_MeshletReocclusion->GetNameHash());

    for (u32 i = 0; i < m_DrawsReocclusion.size(); i++)
    {
        auto& view = m_MultiviewData->View(i);
        Utils::updateRecordedAttachmentResources(
            view.Dynamic.DrawInfo.Attachments, attachmentRenames);
        m_DrawsReocclusion[i]->AddToGraph(renderGraph, {
            .Geometry = view.Static.Geometry,
            .Commands = meshletReocclusionOutput.MultiviewResource->CompactCommands[i],
            .CommandCount = meshletReocclusionOutput.MultiviewResource->CompactCommandCountReocclusion,
            .CountOffset = i,
            .Resolution = view.Dynamic.Resolution,
            .Camera = view.Dynamic.Camera,
            .DrawInfo = {
                .Attachments = view.Dynamic.DrawInfo.Attachments,
               // .SceneLights = *view.Dynamic.SceneLights, // todo: fix me
                .IBL = view.Dynamic.DrawInfo.IBL,
                .SSAO = view.Dynamic.DrawInfo.SSAO}});
        
        auto& drawOutput = renderGraph.GetBlackboard().Get<DrawIndirectCountPass::PassData>(
            m_DrawsReocclusion[i]->GetNameHash());
        Utils::recordUpdatedAttachmentResources(
            view.Dynamic.DrawInfo.Attachments, drawOutput.DrawAttachmentResources,
            attachmentRenames);
    }

    PassData passData = {};
    passData.DrawAttachmentResources.resize(m_DrawsReocclusion.size());
    passData.HiZOut.resize(m_DrawsReocclusion.size());
    for (u32 i = 0; i < m_DrawsReocclusion.size(); i++)
    {
        auto& drawOutput = renderGraph.GetBlackboard().Get<DrawIndirectCountPass::PassData>(
            m_DrawsReocclusion[i]->GetNameHash());
        
        passData.DrawAttachmentResources[i] = drawOutput.DrawAttachmentResources;
        passData.HiZOut[i] = m_MultiviewData->View(i).Static.HiZContext->GetHiZResource();
    }

    renderGraph.GetBlackboard().Update(m_Name.Hash(), passData);
}

void CullMetaMultiviewPass::EnsureViewAttachments(RG::Graph& renderGraph, CullViewDynamicDescription& view)
{
    for (u32 i = 0; i < view.DrawInfo.Attachments.Colors.size(); i++)
    {
        auto& color = view.DrawInfo.Attachments.Colors[i];
        color.Resource = RG::RgUtils::ensureResource(color.Resource, renderGraph,
            std::format("{}.ColorIn.{}", m_Name.Name(), i),
            RG::GraphTextureDescription{
                    .Width = view.Resolution.x,
                    .Height = view.Resolution.y,
                    .Format =  Format::RGBA16_FLOAT});
    }
    
    if (view.DrawInfo.Attachments.Depth.has_value())
    {
        auto& depth = *view.DrawInfo.Attachments.Depth;
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
    for (u32 i = 0; i < view.DrawInfo.Attachments.Colors.size(); i++)
    {
        auto& color = view.DrawInfo.Attachments.Colors[i];
        color.Description.OnLoad = load;
    }
    
    if (view.DrawInfo.Attachments.Depth.has_value())
    {
        auto& depth = *view.DrawInfo.Attachments.Depth;
        depth.Description.OnLoad = load; 
    }
}

