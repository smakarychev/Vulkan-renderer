#include "PbrTCForwardIBLPass.h"

#include "RenderGraph/Passes/Culling/CullMetaPass.h"
#include "RenderGraph/Passes/Culling/CullMetaSinglePass.h"

PbrTCForwardIBLPass::PbrTCForwardIBLPass(RenderGraph::Graph& renderGraph, const PbrForwardIBLPassInitInfo& info,
    std::string_view name)
        : m_Name(name)
{
    ShaderPipelineTemplate* pbrTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/pbr/pbr-ibl-vert.shader",
        "../assets/shaders/processed/render-graph/pbr/pbr-ibl-frag.shader"},
        "Pass.Pbr.Forward.IBL", renderGraph.GetArenaAllocators());

    ShaderPipeline pipeline = ShaderPipeline::Builder()
        .SetTemplate(pbrTemplate)
        .AddSpecialization("MAX_REFLECTION_LOD",
            (f32)Image::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION}))
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT},
            .DepthFormat = Format::D32_FLOAT})
        .UseDescriptorBuffer()
        .Build();

    CullMetaPassInitInfo pbrPassInitInfo = {
        .Geometry = info.Geometry,
        .DrawPipeline = &pipeline,
        .MaterialDescriptors = info.MaterialDescriptors,
        .DrawFeatures = CullMetaPassInitInfo::Features::ShadedIBL};

    m_Pass = std::make_shared<CullMetaPass>(renderGraph, pbrPassInitInfo, name);
}

void PbrTCForwardIBLPass::AddToGraph(RenderGraph::Graph& renderGraph, const PbrForwardIBLPassExecutionInfo& info)
{
    using namespace RenderGraph;

    CullMetaPassExecutionInfo executionInfo = {
        .Resolution = info.Resolution,
        .Colors = {
            CullMetaPassExecutionInfo::ColorInfo{
                .Color = info.ColorIn,
                .OnLoad = info.ColorIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
                .ClearValue = {.Color = {.U = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}}}}},
        .Depth = CullMetaPassExecutionInfo::DepthInfo{
            .Depth = info.DepthIn,
            .OnLoad = info.DepthIn.IsValid() ? AttachmentLoad::Load : AttachmentLoad::Clear,
            .ClearValue = {.DepthStencil = {.Depth = 0.0f, .Stencil = 0}}},
        .IBL = info.IBL};

    m_Pass->AddToGraph(renderGraph, executionInfo);
    auto& output = renderGraph.GetBlackboard().Get<CullMetaPass::PassData>(m_Pass->GetNameHash());
    PassData passData = {
        .ColorOut = output.ColorsOut[0],
        .DepthOut = *output.DepthOut};
    renderGraph.GetBlackboard().Update(m_Name.Hash(), passData);
}
