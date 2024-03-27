#include "CullMetaPass.h"

CullMetaPass::CullMetaPass(RenderGraph::Graph& renderGraph, const CullMetaPassInitInfo& info, std::string_view name)
    : m_Name(name), m_DrawFeatures(info.DrawFeatures)
{
    m_HiZContext = std::make_shared<HiZPassContext>(info.Resolution);
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

    m_TrianglePrepareDispatch = std::make_shared<TrianglePrepareDispatch>(
        renderGraph, m_Name.Name() + ".TriangleCull.PrepareDispatch");
    m_TrianglePrepareReocclusionDispatch = std::make_shared<TrianglePrepareReocclusionDispatch>(
        renderGraph, m_Name.Name() + ".TriangleCull.PrepareDispatch");

    if (m_DrawFeatures == TriangleCullDrawPassInitInfo::Features::Materials ||
        m_DrawFeatures == TriangleCullDrawPassInitInfo::Features::AlphaTest)
    {
        m_ImmutableSamplerDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(info.DrawPipeline->GetTemplate(), DescriptorAllocatorKind::Samplers)
            // todo: make this (0) an enum
            .ExtractSet(0)
            .Build();
    }

    TriangleCullDrawPassInitInfo cullDrawPassInitInfo = {
        .DrawFeatures = m_DrawFeatures,
        .MaterialDescriptors = *info.MaterialDescriptors,
        .ImmutableSamplerDescriptors = m_ImmutableSamplerDescriptors,
        .DrawPipeline = *info.DrawPipeline};
    
    m_CullDraw = std::make_shared<TriangleCullDraw>(renderGraph, cullDrawPassInitInfo, m_Name.Name() + ".CullDraw");
    m_ReoccludeTrianglesDraw = std::make_shared<TriangleReoccludeDraw>(renderGraph, cullDrawPassInitInfo,
        m_Name.Name() + ".CullDraw.TriangleReocclusion");
    m_ReoccludeDraw = std::make_shared<TriangleReoccludeDraw>(renderGraph, cullDrawPassInitInfo,
        m_Name.Name() + ".CullDraw.Reocclusion");
}

CullMetaPass::~CullMetaPass()
{
    m_HiZContext.reset();
}

void CullMetaPass::AddToGraph(RenderGraph::Graph& renderGraph, const CullMetaPassExecutionInfo& info)
{
    using namespace RenderGraph;
    
    auto& blackboard = renderGraph.GetBlackboard();

    auto colors = EnsureColors(renderGraph, info);
    auto depth = EnsureDepth(renderGraph, info);
    
    m_PassData.ColorsOut = colors;
    m_PassData.DepthOut = depth;
    
    m_MeshCull->AddToGraph(renderGraph, *m_MeshContext, *m_HiZContext);
    m_MeshletCull->AddToGraph(renderGraph, *m_MeshletContext);

    // this pass also reads back the number of iterations to cull-draw
    m_TrianglePrepareDispatch->AddToGraph(renderGraph, *m_TriangleContext);
    auto& dispatchOut = blackboard.GetOutput<TrianglePrepareDispatch::PassData>(
        m_TrianglePrepareDispatch->GetNameHash());

    std::vector<TriangleCullDrawPassExecutionInfo::Attachment> colorAttachments;
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
    std::optional<TriangleCullDrawPassExecutionInfo::Attachment> depthAttachment{};
    if (depth.has_value())
        depthAttachment = {
            .Resource = *depth,
            .Description = {
                .Type = RenderingAttachmentType::Depth,
                .Clear = {.DepthStencil = {.Depth = 0.0f}},
                .OnLoad = AttachmentLoad::Clear,
                .OnStore = AttachmentStore::Store}};

    // cull and draw triangles that were visible last frame (this presumably draws most of the triangles)
    m_CullDraw->AddToGraph(renderGraph, {
        .Dispatch = dispatchOut.DispatchIndirect,
        .CullContext = m_TriangleContext.get(),
        .DrawContext = m_TriangleDrawContext.get(),
        .HiZContext = m_HiZContext.get(),
        .Resolution = info.FrameContext->Resolution,
        .ColorAttachments = colorAttachments,
        .DepthAttachment = depthAttachment});

    auto& drawOutput = blackboard.GetOutput<TriangleCullDraw::PassData>(m_CullDraw->GetNameHash());
    m_PassData.ColorsOut = drawOutput.RenderTargets;
    m_HiZ->AddToGraph(renderGraph, drawOutput.DepthTarget.value_or(Resource{}), *m_HiZContext);
    m_PassData.HiZOut = blackboard.GetOutput<HiZPass::PassData>().HiZOut;

    // we have to update attachment resources
    for (u32 i = 0; i < colorAttachments.size(); i++)
    {
        colorAttachments[i].Description.OnLoad = AttachmentLoad::Load;
        colorAttachments[i].Resource = drawOutput.RenderTargets[i];
    }
    if (depthAttachment.has_value())
    {
        depthAttachment->Description.OnLoad = AttachmentLoad::Load;
        depthAttachment->Resource = *drawOutput.DepthTarget;
    }

    // triangle only reocclusion (this updates visibility flags for most of the triangles and draws them)
    m_ReoccludeTrianglesDraw->AddToGraph(renderGraph, {
        .Dispatch = dispatchOut.DispatchIndirect,
        .CullContext = m_TriangleContext.get(),
        .DrawContext = m_TriangleDrawContext.get(),
        .HiZContext = m_HiZContext.get(),
        .Resolution = info.FrameContext->Resolution,
        .ColorAttachments = colorAttachments,
        .DepthAttachment = depthAttachment});
    auto& reoccludeTrianglesOutput = blackboard.GetOutput<TriangleReoccludeDraw::PassData>(
        m_ReoccludeTrianglesDraw->GetNameHash());
    m_PassData.ColorsOut = reoccludeTrianglesOutput.RenderTargets;
    m_HiZReocclusion->AddToGraph(renderGraph,
        reoccludeTrianglesOutput.DepthTarget.value_or(Resource{}), *m_HiZContext);
    m_PassData.HiZOut = blackboard.GetOutput<HiZPass::PassData>().HiZOut;

    // we have to update attachment resources (again)
    for (u32 i = 0; i < colorAttachments.size(); i++)
    {
        colorAttachments[i].Description.OnLoad = AttachmentLoad::Load;
        colorAttachments[i].Resource = reoccludeTrianglesOutput.RenderTargets[i];
    }
    if (depthAttachment.has_value())
    {
        depthAttachment->Description.OnLoad = AttachmentLoad::Load;
        depthAttachment->Resource = *reoccludeTrianglesOutput.DepthTarget;
    }

    // finally, reocclude meshlets and draw them
    m_MeshReocclusion->AddToGraph(renderGraph, *m_MeshContext, *m_HiZContext);
    m_MeshletReocclusion->AddToGraph(renderGraph, *m_MeshletContext);

    // this pass also reads back the number of iterations to cull-draw
    m_TrianglePrepareReocclusionDispatch->AddToGraph(renderGraph, *m_TriangleContext);
    auto& reocclusionDispatchOut = blackboard.GetOutput<TrianglePrepareReocclusionDispatch::PassData>(
        m_TrianglePrepareReocclusionDispatch->GetNameHash());
    // reoccluded-meshlets triangle reocclusion updates visibility of triangles of reoccluded meshlets (and draws them) 
    m_ReoccludeDraw->AddToGraph(renderGraph, {
            .Dispatch = reocclusionDispatchOut.DispatchIndirect,
            .CullContext = m_TriangleContext.get(),
            .DrawContext = m_TriangleDrawContext.get(),
            .HiZContext = m_HiZContext.get(),
            .Resolution = info.FrameContext->Resolution,
            .ColorAttachments = colorAttachments,
            .DepthAttachment = depthAttachment});
    auto& reoccludeOutput = blackboard.GetOutput<TriangleReoccludeDraw::PassData>(
        m_ReoccludeDraw->GetNameHash());
    m_PassData.ColorsOut = reoccludeOutput.RenderTargets;
    m_PassData.DepthOut = reoccludeOutput.DepthTarget;
    
    blackboard.RegisterOutput(m_Name.Hash(), m_PassData);
}

std::vector<RenderGraph::Resource> CullMetaPass::EnsureColors(RenderGraph::Graph& renderGraph,
    const CullMetaPassExecutionInfo& info) const
{
    std::vector<RenderGraph::Resource> colors(info.Colors.size());
    for (u32 i = 0; i < colors.size(); i++)
    {
        RenderGraph::Resource color = info.Colors[i].Color; 
        if (!color.IsValid())
        {
            color = renderGraph.CreateResource(std::format("{}.{}.{}", m_Name.Name(), ".ColorIn", i),
                RenderGraph::GraphTextureDescription{
                    .Width = info.FrameContext->Resolution.x,
                    .Height = info.FrameContext->Resolution.y,
                    .Format =  Format::RGBA16_FLOAT});
        }
        colors[i] = color;
    }
    
    return colors;
}

std::optional<RenderGraph::Resource> CullMetaPass::EnsureDepth(RenderGraph::Graph& renderGraph,
    const CullMetaPassExecutionInfo& info) const
{
    std::optional<RenderGraph::Resource> depth = info.Depth;
    if (depth.has_value() && !depth.value().IsValid())
        depth = renderGraph.CreateResource(m_Name.Name() + ".DepthIn",
            RenderGraph::GraphTextureDescription{
                .Width = info.FrameContext->Resolution.x,
                .Height = info.FrameContext->Resolution.y,
                .Format =  Format::D32_FLOAT});
        
    return depth;
}
