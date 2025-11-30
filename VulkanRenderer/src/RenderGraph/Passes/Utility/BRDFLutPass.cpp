#include "rendererpch.h"

#include "BRDFLutPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/IntegrateBrdfLutBindGroupRG.generated.h"

Passes::BRDFLut::PassData& Passes::BRDFLut::addToGraph(StringId name, RG::Graph& renderGraph, Texture lut)
{
    using namespace RG;

    using PassDataBind = PassDataWithBind<PassData, IntegrateBrdfLutBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("BRDFLut.Setup")

            passData.BindGroup = IntegrateBrdfLutBindGroupRG(graph, graph.SetShader("integrateBrdfLut"_hsv));

            passData.Lut = passData.BindGroup.SetResourcesBrdf(graph.Import("Lut"_hsv, lut));
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("BRDFLut")
            GPU_PROFILE_FRAME("BRDFLut")

            struct PushConstants
            {
                glm::vec2 BRDFResolutionInverse{};
            };
            PushConstants pushConstants = {
                .BRDFResolutionInverse = 1.0f / glm::vec2((f32)BRDF_RESOLUTION)};
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = passData.BindGroup.Shader->GetLayout(), 
            	.Data = {pushConstants}});
            cmd.Dispatch({
                .Invocations = {BRDF_RESOLUTION, BRDF_RESOLUTION, 1},
                .GroupSize = passData.BindGroup.GetComputeMainGroupSize()});
        });
}

TextureDescription Passes::BRDFLut::getLutDescription()
{
    return {
        .Width = BRDF_RESOLUTION,
        .Height = BRDF_RESOLUTION,
        .Format = Format::RG16_FLOAT,
        .Usage = ImageUsage::Sampled | ImageUsage::Storage | ImageUsage::Source
    };
}
