#include "VisibilityPass.h"

VisibilityPass::VisibilityPass(RG::Graph& renderGraph, const VisibilityPassInitInfo& info)
{
    ShaderPipelineTemplate* visibilityTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/general/visibility-buffer-vert.stage",
        "../assets/shaders/processed/render-graph/general/visibility-buffer-frag.stage"},
        "Pass.Visibility", renderGraph.GetArenaAllocators());

    ShaderPipeline::Builder pipelineBuilder = ShaderPipeline::Builder()
        .SetTemplate(visibilityTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::R32_UINT},
            .DepthFormat = Format::D32_FLOAT})
        .AlphaBlending(AlphaBlending::None)
        .UseDescriptorBuffer();
    
    ShaderPipeline trianglePipeline = pipelineBuilder
        .Build();
    ShaderPipeline meshletPipeline = pipelineBuilder
        .AddSpecialization("COMPOUND_INDEX", false)
        .Build();

    m_MultiviewData.AddView({
        .Geometry = info.Geometry,
        .DrawFeatures = 
            RG::DrawFeatures::AlphaTest |
            RG::DrawFeatures::Triangles,
        .DrawMeshletsPipeline = &meshletPipeline,
        .DrawTrianglesPipeline = &trianglePipeline,
        .MaterialDescriptors = info.MaterialDescriptors,
        .CullTriangles = true});

    m_MultiviewData.Finalize();

    CullMetaMultiviewPassInitInfo multiviewPassInitInfo = {
        .MultiviewData = &m_MultiviewData};
    
    m_Pass = std::make_shared<CullMetaMultiviewPass>(renderGraph, "VisibilityPass", multiviewPassInitInfo);
}

void VisibilityPass::AddToGraph(RG::Graph& renderGraph, const VisibilityPassExecutionInfo& info)
{
    using namespace RG;

    Resource visibility = renderGraph.CreateResource("VisibilityBuffer.VisibilityBuffer",
        GraphTextureDescription{
            .Width = info.Resolution.x,
            .Height = info.Resolution.y,
            .Format = Format::R32_UINT});

    Resource depth = renderGraph.CreateResource("VisibilityBuffer.Depth",
        GraphTextureDescription{
            .Width = info.Resolution.x,
            .Height = info.Resolution.y,
            .Format = Format::D32_FLOAT});

    m_MultiviewData.UpdateView(0, {
        .Resolution = info.Resolution,
        .Camera = info.Camera,
        .DrawInfo = {
            .Attachments = {
                .Colors = {DrawAttachment{
                    .Resource = visibility,
                    .Description = {
                        .OnLoad = AttachmentLoad::Clear,
                        .ClearColor = {.U = glm::uvec4{std::numeric_limits<u32>::max(), 0, 0, 0}}}}},
                .Depth = DepthStencilAttachment{
                    .Resource = depth,
                    .Description = {
                        .OnLoad = AttachmentLoad::Clear,
                        .ClearDepth = 0.0f,
                        .ClearStencil = 0}}}}});
    
    m_Pass->AddToGraph(renderGraph);

    auto& output = renderGraph.GetBlackboard().Get<CullMetaMultiviewPass::PassData>(m_Pass->GetNameHash());
    PassData passData = {
        .ColorOut = output.DrawAttachmentResources[0].Colors[0],
        .DepthOut = *output.DrawAttachmentResources[0].Depth,
        .HiZOut = output.HiZOut[0]};
    renderGraph.GetBlackboard().Update(passData);
}
