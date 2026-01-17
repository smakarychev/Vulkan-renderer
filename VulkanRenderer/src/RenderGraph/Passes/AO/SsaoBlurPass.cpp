#include "rendererpch.h"

#include "SsaoBlurPass.h"

#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SsaoBlurBindGroupRG.generated.h"

Passes::SsaoBlur::PassData& Passes::SsaoBlur::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SsaoBlurBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("SSAO.Blur.Setup")

            passData.BindGroup = SsaoBlurBindGroupRG(graph, ShaderDefines({
                ShaderDefine{"VERTICAL"_hsv, info.BlurKind == SsaoBlurPassKind::Vertical}
            }));

            passData.BindGroup.SetResourcesSsao(info.SsaoIn);
            passData.Ssao = passData.BindGroup.SetResourcesSsaoBlurred(
                RgUtils::ensureResource(info.SsaoOut, graph, "ColorOut"_hsv, RGImageDescription{
                    .Inference = RGImageInference::Size,
                    .Reference = info.SsaoIn,
                    .Format = Format::R8_UNORM
            }));
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("SSAO.Blur")
            GPU_PROFILE_FRAME("SSAO.Blur")
            
            auto& ssaoInDescription = graph.GetImageDescription(passData.Ssao);

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
				.Invocations = {ssaoInDescription.Width, ssaoInDescription.Height, 1},
				.GroupSize = passData.BindGroup.GetMainGroupSize()
            });
        });
}
