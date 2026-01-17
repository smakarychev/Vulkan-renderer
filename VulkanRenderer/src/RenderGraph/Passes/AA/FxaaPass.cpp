#include "rendererpch.h"

#include "FxaaPass.h"
#include "RenderGraph/Passes/Generated/FxaaBindGroupRG.generated.h"

Passes::Fxaa::PassData& Passes::Fxaa::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, FxaaBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance.Setup")

            passData.BindGroup = FxaaBindGroupRG(graph);
            
            passData.AntiAliased = graph.Create("AntiAliased"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size,
                .Reference = info.Color,
                .Format = Format::RGBA16_FLOAT
            });

            passData.BindGroup.SetResourcesColor(info.Color);
            passData.AntiAliased = passData.BindGroup.SetResourcesAntialiased(passData.AntiAliased);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Fxaa.Luminance")
            GPU_PROFILE_FRAME("Fxaa.Luminance")

            auto& description = graph.GetImageDescription(passData.AntiAliased);
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
				.Invocations = {description.Width, description.Height, 1},
				.GroupSize = passData.BindGroup.GetMainGroupSize()
            });
        });
}
