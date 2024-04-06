#include "PbrForwardTranslucentIBLPass.h"

#include "RenderGraph/Passes/Culling/MeshletCullTranslucentPass.h"
#include "RenderGraph/Passes/General/DrawIndirectPass.h"

PbrForwardTranslucentIBLPass::PbrForwardTranslucentIBLPass(RG::Graph& renderGraph,
    const PbrForwardTranslucentIBLPassInitInfo& info)
{
    m_MeshContext = std::make_shared<MeshCullContext>(*info.Geometry);
    m_MeshletContext = std::make_shared<MeshletCullTranslucentContext>(*m_MeshContext);

    std::string name = "PBR.Forward.Translucent";
    m_MeshCull = std::make_shared<MeshCullSinglePass>(renderGraph, name + ".MeshCull");
    m_MeshletCull = std::make_shared<MeshletCullTranslucentPass>(renderGraph, name + ".MeshletCull");

    ShaderPipelineTemplate* drawTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/render-graph/pbr/pbr-translucency-vert.shader",
        "../assets/shaders/processed/render-graph/pbr/pbr-translucency-frag.shader"},
        name, renderGraph.GetArenaAllocators());

    ShaderPipeline drawPipeline = ShaderPipeline::Builder()
        .SetTemplate(drawTemplate)
        .DepthMode(DepthMode::Read)
        .FaceCullMode(FaceCullMode::Back)
        .CompatibleWithVertex(VertexP3N3T3UV2::GetInputDescriptionDI())
        .SetRenderingDetails({
            .ColorFormats = {Format::RGBA16_FLOAT},
            .DepthFormat = Format::D32_FLOAT})
        .UseDescriptorBuffer()
        .Build();
    
    m_Draw = std::make_shared<DrawIndirectPass>(renderGraph, name + ".Draw", DrawIndirectPassInitInfo{
        .DrawFeatures = RG::DrawFeatures::ShadedIBL,
        .DrawPipeline = &drawPipeline,
        .MaterialDescriptors = *info.MaterialDescriptors});
}

void PbrForwardTranslucentIBLPass::AddToGraph(RG::Graph& renderGraph,
    const PbrForwardTranslucentIBLPassExecutionInfo& info)
{
    using namespace RG;

    auto& blackboard = renderGraph.GetBlackboard();

    m_MeshCull->AddToGraph(renderGraph, *m_MeshContext, *info.HiZContext);
    m_MeshletCull->AddToGraph(renderGraph, *m_MeshletContext);
    auto& meshletOutput = blackboard.Get<MeshletCullTranslucentPass::PassData>(m_MeshletCull->GetNameHash());

    m_Draw->AddToGraph(renderGraph, m_MeshContext->Geometry(),{
        .Color = info.ColorIn,
        .Depth = info.DepthIn,
        .Commands = meshletOutput.MeshletResources.CommandsSsbo,
        .Resolution = info.Resolution,
        .DepthOnLoad = AttachmentLoad::Load,
        .IBL = info.IBL});
    auto& drawOutput = blackboard.Get<DrawIndirectPass::PassData>(m_Draw->GetNameHash());

    m_PassData.ColorOut = drawOutput.ColorOut;
    m_PassData.DepthOut = drawOutput.DepthOut;
    
    blackboard.Register(m_PassData);
}
