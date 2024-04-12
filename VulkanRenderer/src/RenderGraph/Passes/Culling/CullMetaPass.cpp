#include "CullMetaPass.h"

#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/General/DrawIndirectCountPass.h"

CullMetaPass::CullMetaPass(RG::Graph& renderGraph, const CullMetaPassInitInfo& info, std::string_view name)
    : m_Name(name), m_DrawFeatures(info.DrawFeatures)
{
    m_MeshContext = std::make_shared<MeshCullContext>(*info.Geometry);
    m_MeshletContext = std::make_shared<MeshletCullContext>(*m_MeshContext);
    m_TriangleContext = std::make_shared<TriangleCullContext>(*m_MeshletContext);
    m_TriangleDrawContext = std::make_shared<TriangleDrawContext>();

    m_HiZ = std::make_shared<HiZ>(renderGraph, m_Name.Name() + ".HiZ");
    m_HiZReocclusion = std::make_shared<HiZ>(renderGraph, m_Name.Name() +  ".HiZ.Triangle");

    m_MeshCull = std::make_shared<MeshCull>(renderGraph, m_Name.Name() + ".MeshCull");
    m_MeshReocclusion = std::make_shared<MeshReocclusion>(renderGraph, m_Name.Name() + ".MeshCull");
    m_MeshletCull = std::make_shared<MeshletCull>(renderGraph, m_Name.Name() + ".MeshletCull");
    m_MeshletReocclusion = std::make_shared<MeshletReocclusion>(renderGraph, m_Name.Name() + ".MeshletCull");

    m_TrianglePrepareDispatch = std::make_shared<TriangleCullPrepareDispatchPass>(
        renderGraph, m_Name.Name() + ".TriangleCull.PrepareDispatch");

    TriangleCullDrawPassInitInfo cullDrawPassInitInfo = {
        .DrawFeatures = m_DrawFeatures,
        .DrawTrianglesPipeline = *info.DrawTrianglesPipeline,
        .MaterialDescriptors = *info.MaterialDescriptors};
    
    m_CullDraw = std::make_shared<TriangleCullDraw>(renderGraph, cullDrawPassInitInfo, m_Name.Name() + ".CullDraw");
    m_ReoccludeTrianglesDraw = std::make_shared<TriangleReoccludeDraw>(renderGraph, cullDrawPassInitInfo,
        m_Name.Name() + ".CullDraw.TriangleReocclusion");
    m_ReoccludeDraw = std::make_shared<TriangleReoccludeDraw>(renderGraph, cullDrawPassInitInfo,
        m_Name.Name() + ".CullDraw.Reocclusion");

    m_DrawIndirectCountPass = std::make_shared<DrawIndirectCountPass>(renderGraph,
        m_Name.Name() + ".CullDraw.MeshletReocclusion", DrawIndirectCountPassInitInfo{
            .DrawFeatures = m_DrawFeatures,
            .DrawPipeline = *info.DrawMeshletsPipeline,
            .MaterialDescriptors = *info.MaterialDescriptors});
}

void CullMetaPass::AddToGraph(RG::Graph& renderGraph, const CullMetaPassExecutionInfo& info)
{
    using namespace RG;

    if (!m_HiZContext ||
        m_HiZContext->GetDrawResolution().x != info.Resolution.x ||
        m_HiZContext->GetDrawResolution().y != info.Resolution.y)
    {
        m_HiZContext = std::make_shared<HiZPassContext>(info.Resolution, renderGraph.GetResolutionDeletionQueue());
    }

    auto& blackboard = renderGraph.GetBlackboard();

    auto colors = EnsureColors(renderGraph, info, m_Name);
    auto depth = EnsureDepth(renderGraph, info, m_Name);
    
    m_PassData.DrawAttachmentResources.RenderTargets = colors;
    m_PassData.DrawAttachmentResources.DepthTarget = depth;
    
    m_MeshCull->AddToGraph(renderGraph, *m_MeshContext, *m_HiZContext);
    m_MeshletCull->AddToGraph(renderGraph, *m_MeshletContext);

    // this pass also reads back the number of iterations to cull-draw
    m_TrianglePrepareDispatch->AddToGraph(renderGraph, *m_TriangleContext);
    auto& dispatchOut = blackboard.Get<TriangleCullPrepareDispatchPass::PassData>(
        m_TrianglePrepareDispatch->GetNameHash());

    std::vector<DrawAttachment> colorAttachments;
    colorAttachments.reserve(colors.size());
    for (u32 i = 0; i < colors.size(); i++)
    {
        auto& color = colors[i];
        colorAttachments.push_back({
            .Resource = color,
            .Description = {
                .Type = RenderingAttachmentType::Color,
                .Clear = info.Colors[i].ClearValue,
                .OnLoad = info.Colors[i].OnLoad,
                .OnStore = AttachmentStore::Store}});
    }
    std::optional<DrawAttachment> depthAttachment{};
    if (info.Depth.has_value())
        depthAttachment = {
            .Resource = *depth,
            .Description = {
                .Type = RenderingAttachmentType::Depth,
                .Clear = info.Depth->ClearValue,
                .OnLoad = info.Depth->OnLoad,
                .OnStore = AttachmentStore::Store}};

    // cull and draw triangles that were visible last frame (this presumably draws most of the triangles)
    m_CullDraw->AddToGraph(renderGraph, {
        .Dispatch = dispatchOut.DispatchIndirect,
        .CompactCount = dispatchOut.CompactCountSsbo,
        .CullContext = m_TriangleContext.get(),
        .DrawContext = m_TriangleDrawContext.get(),
        .HiZContext = m_HiZContext.get(),
        .Resolution = info.Resolution,
        .DrawAttachments = {
            .ColorAttachments = colorAttachments,
            .DepthAttachment = depthAttachment},
        .IBL = info.IBL,
        .SSAO = info.SSAO});

    auto& drawOutput = blackboard.Get<TriangleCullDraw::PassData>(m_CullDraw->GetNameHash());
    m_HiZ->AddToGraph(renderGraph, drawOutput.DrawAttachmentResources.DepthTarget.value_or(Resource{}), *m_HiZContext);
    m_PassData.HiZOut = blackboard.Get<HiZPass::PassData>().HiZOut;

    // we have to update attachment resources
    for (u32 i = 0; i < colorAttachments.size(); i++)
    {
        colorAttachments[i].Description.OnLoad = AttachmentLoad::Load;
        colorAttachments[i].Resource = drawOutput.DrawAttachmentResources.RenderTargets[i];
    }
    if (depthAttachment.has_value())
    {
        depthAttachment->Description.OnLoad = AttachmentLoad::Load;
        depthAttachment->Resource = *drawOutput.DrawAttachmentResources.DepthTarget;
    }

    // triangle only reocclusion (this updates visibility flags for most of the triangles and draws them)
    m_ReoccludeTrianglesDraw->AddToGraph(renderGraph, {
        .Dispatch = dispatchOut.DispatchIndirect,
        .CompactCount = dispatchOut.CompactCountSsbo,
        .CullContext = m_TriangleContext.get(),
        .DrawContext = m_TriangleDrawContext.get(),
        .HiZContext = m_HiZContext.get(),
        .Resolution = info.Resolution,
        .DrawAttachments = {
            .ColorAttachments = colorAttachments,
            .DepthAttachment = depthAttachment},
        .IBL = info.IBL,
        .SSAO = info.SSAO});
    
    auto& reoccludeTrianglesOutput = blackboard.Get<TriangleReoccludeDraw::PassData>(
        m_ReoccludeTrianglesDraw->GetNameHash());
    m_HiZReocclusion->AddToGraph(renderGraph,
        reoccludeTrianglesOutput.DrawAttachmentResources.DepthTarget.value_or(Resource{}), *m_HiZContext);
    m_PassData.HiZOut = blackboard.Get<HiZPass::PassData>().HiZOut;

    // we have to update attachment resources (again)
    for (u32 i = 0; i < colorAttachments.size(); i++)
    {
        colorAttachments[i].Description.OnLoad = AttachmentLoad::Load;
        colorAttachments[i].Resource = reoccludeTrianglesOutput.DrawAttachmentResources.RenderTargets[i];
    }
    if (depthAttachment.has_value())
    {
        depthAttachment->Description.OnLoad = AttachmentLoad::Load;
        depthAttachment->Resource = *reoccludeTrianglesOutput.DrawAttachmentResources.DepthTarget;
    }

    // finally, reocclude meshlets and draw them
    m_MeshReocclusion->AddToGraph(renderGraph, *m_MeshContext, *m_HiZContext);
    m_MeshletReocclusion->AddToGraph(renderGraph, *m_MeshletContext);
    auto& meshletReocclusionOutput = blackboard.Get<MeshletReocclusion::PassData>(m_MeshletReocclusion->GetNameHash());

    m_DrawIndirectCountPass->AddToGraph(renderGraph, {
        .Geometry = &m_MeshContext->Geometry(),
        .Commands = meshletReocclusionOutput.MeshletResources.CompactCommandsSsbo,
        .CommandCount = meshletReocclusionOutput.MeshletResources.CompactCountReocclusionSsbo,
        .Resolution = info.Resolution,
        .DrawAttachments = {
            .ColorAttachments = colorAttachments,
            .DepthAttachment = depthAttachment},
        .IBL = info.IBL,
        .SSAO = info.SSAO});

    auto& reoccludeOutput = blackboard.Get<DrawIndirectCountPass::PassData>(
        m_DrawIndirectCountPass->GetNameHash());
    m_PassData.DrawAttachmentResources = reoccludeOutput.DrawAttachmentResources;
    
    blackboard.Register(m_Name.Hash(), m_PassData);
}

std::vector<RG::Resource> CullMetaPass::EnsureColors(RG::Graph& renderGraph,
    const CullMetaPassExecutionInfo& info, const RG::PassName& name)
{
    std::vector<RG::Resource> colors(info.Colors.size());
    for (u32 i = 0; i < colors.size(); i++)
        colors[i] = RG::RgUtils::ensureResource(
            info.Colors[i].Color, renderGraph, std::format("{}.{}.{}", name.Name(), ".ColorIn", i),
                RG::GraphTextureDescription{
                    .Width = info.Resolution.x,
                    .Height = info.Resolution.y,
                    .Format =  Format::RGBA16_FLOAT});
    
    return colors;
}

std::optional<RG::Resource> CullMetaPass::EnsureDepth(RG::Graph& renderGraph,
    const CullMetaPassExecutionInfo& info, const RG::PassName& name)
{
    if (!info.Depth.has_value())
        return {};
    std::optional depth = info.Depth->Depth;
    if (depth.has_value())
        depth = RG::RgUtils::ensureResource(*depth, renderGraph, name.Name() + ".DepthIn",
            RG::GraphTextureDescription{
                .Width = info.Resolution.x,
                .Height = info.Resolution.y,
                .Format =  Format::D32_FLOAT});
        
    return depth;
}
