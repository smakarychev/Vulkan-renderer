#include "SsaoBlurPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SsaoBlurBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::SsaoBlur::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource ssao,
    RG::Resource colorOut, SsaoBlurPassKind kind)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    Pass& pass = renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Blur.Setup")

            graph.SetShader("ssao-blur.shader",
                ShaderOverrides{
                    ShaderOverride{"IS_VERTICAL"_hsv, kind == SsaoBlurPassKind::Vertical}});
            
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
            
            auto&& [ssaoIn, ssaoInDescription] = resources.GetTextureWithDescription(passData.SsaoIn);
            Texture ssaoOut = resources.GetTexture(passData.SsaoOut);

            const Shader& shader = resources.GetGraph()->GetShader();
            SsaoBlurShaderBindGroup bindGroup(shader);
            bindGroup.SetSsao({.Image = ssaoIn}, ImageLayout::Readonly);
            bindGroup.SetSsaoBlurred({.Image = ssaoOut}, ImageLayout::General);
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            frameContext.CommandList.Dispatch({
				.Invocations = {ssaoInDescription.Width, ssaoInDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        });

    return pass;
}
