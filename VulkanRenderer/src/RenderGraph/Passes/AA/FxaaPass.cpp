#include "FxaaPass.h"

#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Fxaa::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource colorIn)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance.Setup");

            graph.SetShader("../assets/shaders/fxaa.shader");

            auto& description = graph.GetTextureDescription(colorIn);
            passData.AntiAliased = graph.CreateResource(std::format("{}.AntiAliased", name), GraphTextureDescription{
                .Width = description.Width,
                .Height = description.Height,
                .Format = Format::RGBA16_FLOAT});
            
            passData.ColorIn = graph.Read(colorIn, Compute | Sampled);
            passData.AntiAliased = graph.Write(passData.AntiAliased, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance");
            GPU_PROFILE_FRAME("Fxaa.Luminance");

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            const Texture& input = resources.GetTexture(passData.ColorIn);
            resourceDescriptors.UpdateBinding("u_color", resources.GetTexture(passData.ColorIn).BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_antialiased", resources.GetTexture(passData.AntiAliased).BindingInfo(
                ImageFilter::Linear, ImageLayout::General));

            samplerDescriptors.BindComputeImmutableSamplers(frameContext.Cmd, shader.GetLayout());
            pipeline.BindCompute(frameContext.Cmd);
            resourceDescriptors.BindCompute(frameContext.Cmd, resources.GetGraph()->GetArenaAllocators(),
                shader.GetLayout());
            RenderCommand::Dispatch(frameContext.Cmd,
                {input.Description().Width, input.Description().Height, 1},
                {16, 16, 1});
        });
}
