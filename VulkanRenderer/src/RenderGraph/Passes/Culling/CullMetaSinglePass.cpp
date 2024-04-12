#include "CullMetaSinglePass.h"

#include "CullMetaPass.h"

CullMetaSinglePass::CullMetaSinglePass(RG::Graph& renderGraph, const CullMetaSinglePassInitInfo& info,
    std::string_view name)
        : m_Name(name), m_DrawFeatures(info.DrawFeatures)
{
    m_MeshContext = std::make_shared<MeshCullContext>(*info.Geometry);
    m_MeshletContext = std::make_shared<MeshletCullContext>(*m_MeshContext);
    m_TriangleContext = std::make_shared<TriangleCullContext>(*m_MeshletContext);
    m_TriangleDrawContext = std::make_shared<TriangleDrawContext>();

    m_MeshCull = std::make_shared<MeshCull>(renderGraph, m_Name.Name() + ".MeshCull");
    m_MeshletCull = std::make_shared<MeshletCull>(renderGraph, m_Name.Name() + ".MeshletCull");

    m_TrianglePrepareDispatch = std::make_shared<TriangleCullPrepareDispatchPass>(
        renderGraph, m_Name.Name() + ".TriangleCull.PrepareDispatch");

    TriangleCullDrawPassInitInfo cullDrawPassInitInfo = {
        .DrawFeatures = m_DrawFeatures,
        .DrawTrianglesPipeline = *info.DrawPipeline,
        .MaterialDescriptors = *info.MaterialDescriptors};
    
    m_CullDraw = std::make_shared<TriangleCullDraw>(renderGraph, cullDrawPassInitInfo, m_Name.Name() + ".CullDraw");
}

void CullMetaSinglePass::AddToGraph(RG::Graph& renderGraph, const CullMetaPassExecutionInfo& info,
    HiZPassContext& hiZContext)
{
    using namespace RG;

    auto& blackboard = renderGraph.GetBlackboard();

    auto colors = CullMetaPass::EnsureColors(renderGraph, info, m_Name);
    auto depth = CullMetaPass::EnsureDepth(renderGraph, info, m_Name);

    m_MeshCull->AddToGraph(renderGraph, *m_MeshContext, hiZContext);
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

    m_CullDraw->AddToGraph(renderGraph, {
        .Dispatch = dispatchOut.DispatchIndirect,
        .CullContext = m_TriangleContext.get(),
        .DrawContext = m_TriangleDrawContext.get(),
        .HiZContext = &hiZContext,
        .Resolution = info.Resolution,
        .DrawAttachments = {
            .ColorAttachments = colorAttachments,
            .DepthAttachment = depthAttachment},
        .IBL = info.IBL,
        .SSAO = info.SSAO});

    auto& drawOutput = blackboard.Get<TriangleCullDraw::PassData>(m_CullDraw->GetNameHash());

    m_PassData.DrawAttachmentResources.RenderTargets = drawOutput.DrawAttachmentResources.RenderTargets;
    m_PassData.DrawAttachmentResources.DepthTarget = drawOutput.DrawAttachmentResources.DepthTarget;
    
    blackboard.Register(m_Name.Hash(), m_PassData);
}
