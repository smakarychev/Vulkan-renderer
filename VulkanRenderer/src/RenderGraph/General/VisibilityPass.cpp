#include "VisibilityPass.h"

#include "RenderGraph/Culling/CullMetaPass.h"

VisibilityPass::VisibilityPass(RenderGraph::Graph& renderGraph, const VisibilityPassInitInfo& info)
{
    ShaderPipelineTemplate* visibilityTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/general/visibility-buffer-vert.shader",
        "../assets/shaders/processed/render-graph/general/visibility-buffer-frag.shader",},
        "Pass.Visibility", renderGraph.GetArenaAllocators());

    ShaderPipeline pipeline = ShaderPipeline::Builder()
        .SetTemplate(visibilityTemplate)
        .SetRenderingDetails({
            .ColorFormats = {Format::R32_UINT},
            .DepthFormat = Format::D32_FLOAT})
        .AlphaBlending(AlphaBlending::None)
        .UseDescriptorBuffer()
        .Build();

    CullMetaPassInitInfo visibilityPassInitInfo = {
        .Geometry = info.Geometry,
        .DrawPipeline = &pipeline,
        .MaterialDescriptors = info.MaterialDescriptors,
        .DrawFeatures = CullMetaPassInitInfo::Features::AlphaTest};

    m_Pass = std::make_shared<CullMetaPass>(renderGraph, visibilityPassInitInfo, "VisibilityBuffer");
}

void VisibilityPass::AddToGraph(RenderGraph::Graph& renderGraph, const glm::uvec2& resolution)
{
    using namespace RenderGraph;

    Resource visibility =renderGraph.CreateResource("VisibilityBuffer.VisibilityBuffer",
        GraphTextureDescription{
            .Width = resolution.x,
            .Height = resolution.y,
            .Format = Format::R32_UINT});

    m_Pass->AddToGraph(renderGraph, {
        .Resolution = resolution,
        .Colors = {
            CullMetaPassExecutionInfo::ColorInfo{
                .Color = visibility,
                .OnLoad = AttachmentLoad::Clear,
                .ClearValue = {.Color = {.U = glm::uvec4{std::numeric_limits<u32>::max(), 0, 0, 0}}}}},
        .Depth = Resource{}});

    auto& output = renderGraph.GetBlackboard().GetOutput<CullMetaPass::PassData>(m_Pass->GetNameHash());
    PassData passData = {
        .ColorsOut = output.ColorsOut[0],
        .DepthOut = *output.DepthOut,
        .HiZOut = output.HiZOut};
    renderGraph.GetBlackboard().UpdateOutput(passData);
}
