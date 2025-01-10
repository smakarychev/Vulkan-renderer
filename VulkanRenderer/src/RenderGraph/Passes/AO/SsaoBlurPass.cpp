#include "SsaoBlurPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::SsaoBlur::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource ssao,
    RG::Resource colorOut, SsaoBlurPassKind kind)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Blur.Setup")

            graph.SetShader("../assets/shaders/ssao-blur.shader",
                ShaderOverrides{
                    ShaderOverride{{"IS_VERTICAL"}, kind == SsaoBlurPassKind::Vertical}});
            
            const TextureDescription& ssaoDescription = Resources(graph).GetTextureDescription(ssao);
            passData.SsaoOut = RgUtils::ensureResource(colorOut, graph, std::string{name} + ".ColorOut",
                GraphTextureDescription{
                    .Width = ssaoDescription.Width,
                    .Height = ssaoDescription.Height,
                    .Format = Format::R8_UNORM});

            passData.SsaoIn = graph.Read(ssao, Compute | Sampled);
            passData.SsaoOut = graph.Write(passData.SsaoOut, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("SSAO.Blur")
            GPU_PROFILE_FRAME("SSAO.Blur")
            
            const Texture& ssaoIn = resources.GetTexture(passData.SsaoIn);
            const Texture& ssaoOut = resources.GetTexture(passData.SsaoOut);

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_ssao", ssaoIn.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors.UpdateBinding("u_ssao_blurred", ssaoOut.BindingInfo(
                ImageFilter::Linear, ImageLayout::General));
            
            auto& cmd = frameContext.Cmd;
            samplerDescriptors.BindComputeImmutableSamplers(cmd, shader.GetLayout());
            pipeline.BindCompute(cmd);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::Dispatch(cmd,
                {ssaoIn.Description().Width, ssaoIn.Description().Height, 1},
                {16, 16, 1});
        });

    return pass;
}
