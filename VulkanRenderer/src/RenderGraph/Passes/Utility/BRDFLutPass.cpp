#include "BRDFLutPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/BrdfLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::BRDFLut::PassData& Passes::BRDFLut::addToGraph(StringId name, RG::Graph& renderGraph, Texture lut)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("BRDFLut.Setup")

            graph.SetShader("brdf-lut"_hsv);

            passData.Lut = graph.Import("Lut"_hsv, lut);

            passData.Lut = graph.WriteImage(passData.Lut, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("BRDFLut")
            GPU_PROFILE_FRAME("BRDFLut")

            const Shader& shader = graph.GetShader();
            BrdfLutShaderBindGroup bindGroup(shader);

            bindGroup.SetBrdf(graph.GetImageBinding(passData.Lut));

            struct PushConstants
            {
                glm::vec2 BRDFResolutionInverse{};
            };
            PushConstants pushConstants = {
                .BRDFResolutionInverse = 1.0f / glm::vec2((f32)BRDF_RESOLUTION)};
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
            	.PipelineLayout = shader.GetLayout(), 
            	.Data = {pushConstants}});
            cmd.Dispatch({
                .Invocations = {BRDF_RESOLUTION, BRDF_RESOLUTION, 1},
                .GroupSize = {32, 32, 1}});
        });
}

TextureDescription Passes::BRDFLut::getLutDescription()
{
    return {
        .Width = BRDF_RESOLUTION,
        .Height = BRDF_RESOLUTION,
        .Format = Format::RG16_FLOAT,
        .Usage = ImageUsage::Sampled | ImageUsage::Storage};
}
