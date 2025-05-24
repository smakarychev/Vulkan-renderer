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
            
            passData.SsaoOut = RgUtils::ensureResource(info.SsaoOut, graph, "ColorOut"_hsv,
                RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = info.SsaoIn,
                    .Format = Format::R8_UNORM});

            passData.SsaoIn = graph.ReadImage(info.SsaoIn, Compute | Sampled);
            passData.SsaoOut = graph.WriteImage(passData.SsaoOut, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("SSAO.Blur")
            GPU_PROFILE_FRAME("SSAO.Blur")
            
            auto&& [ssaoIn, ssaoInDescription] = graph.GetImageWithDescription(passData.SsaoIn);

            const Shader& shader = graph.GetShader();
            SsaoBlurShaderBindGroup bindGroup(shader);
            bindGroup.SetSsao(graph.GetImageBinding(passData.SsaoIn));
            bindGroup.SetSsaoBlurred(graph.GetImageBinding(passData.SsaoOut));
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {ssaoInDescription.Width, ssaoInDescription.Height, 1},
				.GroupSize = {16, 16, 1}});
        });
}
