#include "VisibilityPass.h"

VisibilityPass::VisibilityPass(RG::Graph& renderGraph, const VisibilityPassInitInfo& info)
{
    ShaderPipelineTemplate* visibilityTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/general/visibility-buffer-vert.shader",
        "../assets/shaders/processed/render-graph/general/visibility-buffer-frag.shader"},
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

    CullMetaPassInitInfo visibilityPassInitInfo = {
        .Geometry = info.Geometry,
        .DrawTrianglesPipeline = &trianglePipeline,
        .DrawMeshletsPipeline = &meshletPipeline,
        .MaterialDescriptors = info.MaterialDescriptors,
        .DrawFeatures =
            RG::DrawFeatures::AlphaTest |
            RG::DrawFeatures::Triangles,
        .CameraType = info.CameraType};

    m_Pass = std::make_shared<CullMetaPass>(renderGraph, visibilityPassInitInfo, "VisibilityBuffer");
}

void VisibilityPass::AddToGraph(RG::Graph& renderGraph, const VisibilityPassExecutionInfo& info)
{
    using namespace RG;

    Resource visibility = renderGraph.CreateResource("VisibilityBuffer.VisibilityBuffer",
        GraphTextureDescription{
            .Width = info.Resolution.x,
            .Height = info.Resolution.y,
            .Format = Format::R32_UINT});

    m_Pass->AddToGraph(renderGraph, {
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
                    .Resource = {},
                    .Description = {
                        .OnLoad = AttachmentLoad::Clear,
                        .ClearDepth = 0.0f,
                        .ClearStencil = 0}}}}});

    auto& output = renderGraph.GetBlackboard().Get<CullMetaPass::PassData>(m_Pass->GetNameHash());
    PassData passData = {
        .ColorOut = output.DrawAttachmentResources.Colors[0],
        .DepthOut = *output.DrawAttachmentResources.Depth,
        .HiZOut = output.HiZOut};
    renderGraph.GetBlackboard().Update(passData);
}
