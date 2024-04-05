#include "VisibilityPass.h"

VisibilityPass::VisibilityPass(RG::Graph& renderGraph, const VisibilityPassInitInfo& info)
{
    ShaderPipelineTemplate* visibilityTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/general/visibility-buffer-vert.shader",
        "../assets/shaders/processed/render-graph/general/visibility-buffer-frag.shader"},
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
        .DrawFeatures =
            CullMetaPassInitInfo::Features::AlphaTest |
            RG::DrawFeatures::Triangles};

    m_Pass = std::make_shared<CullMetaPass>(renderGraph, visibilityPassInitInfo, "VisibilityBuffer");
}

void VisibilityPass::AddToGraph(RG::Graph& renderGraph, const glm::uvec2& resolution)
{
    using namespace RG;

    Resource visibility = renderGraph.CreateResource("VisibilityBuffer.VisibilityBuffer",
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
        .Depth = CullMetaPassExecutionInfo::DepthInfo{
            .Depth = {},
            .OnLoad = AttachmentLoad::Clear,
            .ClearValue = {.DepthStencil = {.Depth = 0.0f, .Stencil = 0}}}});

    auto& output = renderGraph.GetBlackboard().Get<CullMetaPass::PassData>(m_Pass->GetNameHash());
    PassData passData = {
        .ColorOut = output.ColorsOut[0],
        .DepthOut = *output.DepthOut,
        .HiZOut = output.HiZOut};
    renderGraph.GetBlackboard().Update(passData);
}
