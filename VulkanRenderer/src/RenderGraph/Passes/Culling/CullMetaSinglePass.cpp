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

    m_MeshCull = std::make_shared<MeshCull>(renderGraph, m_Name.Name() + ".MeshCull", MeshCullPassInitInfo{
        .ClampDepth = info.ClampDepth});
    m_MeshletCull = std::make_shared<MeshletCull>(renderGraph, m_Name.Name() + ".MeshletCull", MeshletCullPassInitInfo{
        .ClampDepth = info.ClampDepth,
        .CameraType = info.CameraType});

    m_TrianglePrepareDispatch = std::make_shared<TriangleCullPrepareDispatchPass>(
        renderGraph, m_Name.Name() + ".TriangleCull.PrepareDispatch");

    TriangleCullDrawPassInitInfo cullDrawPassInitInfo = {
        .DrawFeatures = m_DrawFeatures,
        .DrawTrianglesPipeline = *info.DrawPipeline,
        .MaterialDescriptors = info.MaterialDescriptors};
    
    m_CullDraw = std::make_shared<TriangleCullDraw>(renderGraph, m_Name.Name() + ".CullDraw", cullDrawPassInitInfo);
}

void CullMetaSinglePass::AddToGraph(RG::Graph& renderGraph, const CullMetaPassExecutionInfo& info,
    HiZPassContext& hiZContext)
{
    using namespace RG;

    m_MeshContext->SetCamera(info.Camera);

    auto& blackboard = renderGraph.GetBlackboard();

    auto colors = CullMetaPass::EnsureColors(renderGraph, info, m_Name);
    auto depth = CullMetaPass::EnsureDepth(renderGraph, info, m_Name);

    m_MeshCull->AddToGraph(renderGraph, *m_MeshContext, hiZContext);
    m_MeshletCull->AddToGraph(renderGraph, *m_MeshletContext);

    // this pass also reads back the number of iterations to cull-draw
    m_TrianglePrepareDispatch->AddToGraph(renderGraph, *m_TriangleContext);
    auto& dispatchOut = blackboard.Get<TriangleCullPrepareDispatchPass::PassData>(
        m_TrianglePrepareDispatch->GetNameHash());

    std::vector<DrawAttachment> colorAttachments = info.DrawAttachments.Colors;
    for (u32 i = 0; i < colors.size(); i++)
        colorAttachments[i].Resource = colors[i];
    std::optional<DepthStencilAttachment> depthAttachment = info.DrawAttachments.Depth;
    if (depthAttachment.has_value())
        depthAttachment->Resource = *depth;

    m_CullDraw->AddToGraph(renderGraph, {
        .Dispatch = dispatchOut.DispatchIndirect,
        .CullContext = m_TriangleContext.get(),
        .DrawContext = m_TriangleDrawContext.get(),
        .HiZContext = &hiZContext,
        .Resolution = info.Resolution,
        .DrawAttachments = {
            .Colors = colorAttachments,
            .Depth = depthAttachment},
        .IBL = info.IBL,
        .SSAO = info.SSAO});

    auto& drawOutput = blackboard.Get<TriangleCullDraw::PassData>(m_CullDraw->GetNameHash());

    m_PassData.DrawAttachmentResources.RenderTargets = drawOutput.DrawAttachmentResources.RenderTargets;
    m_PassData.DrawAttachmentResources.DepthTarget = drawOutput.DrawAttachmentResources.DepthTarget;
    
    blackboard.Register(m_Name.Hash(), m_PassData);
}
