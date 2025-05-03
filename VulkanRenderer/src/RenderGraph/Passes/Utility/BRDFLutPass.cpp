#include "BRDFLutPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/BrdfLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::BRDFLut::addToGraph(StringId name, RG::Graph& renderGraph, Texture lut)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("BRDFLut.Setup")

            graph.SetShader("brdf-lut"_hsv);

            passData.Lut = graph.AddExternal("Lut"_hsv, lut);

            passData.Lut = graph.Write(passData.Lut, Compute | Storage);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("BRDFLut")
            GPU_PROFILE_FRAME("BRDFLut")

            Texture lutTexture = resources.GetTexture(passData.Lut);

            const Shader& shader = resources.GetGraph()->GetShader();
            BrdfLutShaderBindGroup bindGroup(shader);

            bindGroup.SetBrdf({.Image = lutTexture}, ImageLayout::General);

            struct PushConstants
            {
                glm::vec2 BRDFResolutionInverse{};
            };
            PushConstants pushConstants = {
                .BRDFResolutionInverse = 1.0f / glm::vec2((f32)BRDF_RESOLUTION)};
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
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
