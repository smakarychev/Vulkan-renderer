#include "BRDFLutPass.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/Passes/Generated/BrdfLutBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::BRDFLut::addToGraph(std::string_view name, RG::Graph& renderGraph, const Texture& lut)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("BRDFLut.Setup")

            graph.SetShader("brdf-lut.shader");

            passData.Lut = graph.AddExternal(std::format("{}.Lut", name), lut);

            passData.Lut = graph.Write(passData.Lut, Compute | Storage);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("BRDFLut")
            GPU_PROFILE_FRAME("BRDFLut")

            const Texture& lutTexture = resources.GetTexture(passData.Lut);

            const Shader& shader = resources.GetGraph()->GetShader();
            BrdfLutShaderBindGroup bindGroup(shader);

            bindGroup.SetBrdf(lutTexture.BindingInfo(ImageFilter::Linear, ImageLayout::General));

            struct PushConstants
            {
                glm::vec2 BRDFResolutionInverse{};
            };
            PushConstants pushConstants = {
                .BRDFResolutionInverse = 1.0f / glm::vec2((f32)BRDF_RESOLUTION)};
            
            auto& cmd = frameContext.Cmd;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstants);
            RenderCommand::Dispatch(cmd,
                {BRDF_RESOLUTION, BRDF_RESOLUTION, 1},
                {32, 32, 1});
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
