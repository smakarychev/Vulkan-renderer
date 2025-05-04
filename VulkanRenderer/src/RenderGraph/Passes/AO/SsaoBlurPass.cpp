#include "SsaoBlurPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SsaoBlurBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::SsaoBlur::PassData& Passes::SsaoBlur::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Blur.Setup")

            graph.SetShader("ssao-blur"_hsv,
                ShaderSpecializations{
                    ShaderSpecialization{"IS_VERTICAL"_hsv, info.BlurKind == SsaoBlurPassKind::Vertical}});
            
            const TextureDescription& ssaoDescription = Resources(graph).GetTextureDescription(info.SsaoIn);
            passData.SsaoOut = RgUtils::ensureResource(info.SsaoOut, graph, "ColorOut"_hsv,
                GraphTextureDescription{
                    .Width = ssaoDescription.Width,
                    .Height = ssaoDescription.Height,
                    .Format = Format::R8_UNORM});

            passData.SsaoIn = graph.Read(info.SsaoIn, Compute | Sampled);
            passData.SsaoOut = graph.Write(passData.SsaoOut, Compute | Storage);
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
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {ssaoInDescription.Width, ssaoInDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        }).Data;
}
