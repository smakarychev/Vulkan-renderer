#include "DirectionalShadowPass.h"

DirectionalShadowPass::DirectionalShadowPass(RG::Graph& renderGraph, const DirectionalShadowPassInitInfo& info)
{
    ShaderPipelineTemplate* shadowTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/shadows/directional-vert.shader"},
        "Pass.Shadow.Directional", renderGraph.GetArenaAllocators());

    ShaderPipeline::Builder pipelineBuilder = ShaderPipeline::Builder()
        .SetTemplate(shadowTemplate)
        .SetRenderingDetails({
            .DepthFormat = Format::D32_FLOAT})
        .AlphaBlending(AlphaBlending::None)
        .UseDescriptorBuffer();
    
    ShaderPipeline trianglePipeline = pipelineBuilder
        .Build();
    ShaderPipeline meshletPipeline = pipelineBuilder
        .AddSpecialization("COMPOUND_INDEX", false)
        .Build();

    CullMetaPassInitInfo shaderPassInitInfo = {
        .Geometry = info.Geometry,
        .DrawTrianglesPipeline = &trianglePipeline,
        .DrawMeshletsPipeline = &meshletPipeline,
        .DrawFeatures = RG::DrawFeatures::Positions,
        .CameraType = CameraType::Orthographic};

    m_Pass = std::make_shared<CullMetaPass>(renderGraph, shaderPassInitInfo, "DirectionalShadow");
}

void DirectionalShadowPass::AddToGraph(RG::Graph& renderGraph, const DirectionalShadowPassExecutionInfo& info)
{
    using namespace RG;

    ASSERT(info.Camera->GetType() == CameraType::Orthographic, "DirectionalShadowPass assumes orthographic projection")
    // todo: to cvar
    static constexpr u32 SHADOW_MAP_RESOLUTION = 2048;
    
    Resource shadow = renderGraph.CreateResource("DirectionalShadow.ShadowMap",
        GraphTextureDescription{
            .Width = SHADOW_MAP_RESOLUTION,
            .Height = SHADOW_MAP_RESOLUTION,
            .Format = Format::D32_FLOAT});

    m_Pass->AddToGraph(renderGraph, {
        .Resolution = glm::uvec2{SHADOW_MAP_RESOLUTION},
        .Camera = info.Camera,
        .Depth = CullMetaPassExecutionInfo::DepthInfo{
            .Depth = shadow,
            .OnLoad = AttachmentLoad::Clear,
            .ClearValue = {.DepthStencil = {.Depth = 0.0f, .Stencil = 0}}}});

    auto& output = renderGraph.GetBlackboard().Get<CullMetaPass::PassData>(m_Pass->GetNameHash());
    PassData passData = {
        .ShadowMap = *output.DrawAttachmentResources.DepthTarget};
    renderGraph.GetBlackboard().Update(passData);
}
