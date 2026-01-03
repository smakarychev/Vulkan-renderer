#include "rendererpch.h"

#include "AtmosphereSkyViewLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/AtmosphereLutSkyviewBindGroupRG.generated.h"

Passes::Atmosphere::SkyView::PassData& Passes::Atmosphere::SkyView::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, AtmosphereLutSkyviewBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView.Setup")

            passData.BindGroup = AtmosphereLutSkyviewBindGroupRG(graph);

            passData.Lut = passData.BindGroup.SetResourcesSkyView(graph.Create("Lut"_hsv, RGImageDescription{
                .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.SkyView.Width"_hsv),
                .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.SkyView.Height"_hsv),
                .Format = Format::RGBA16_FLOAT
            }));

            passData.BindGroup.SetResourcesTransmittanceLut(info.TransmittanceLut);
            passData.BindGroup.SetResourcesMultiscatteringLut(info.MultiscatteringLut);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.SkyView")
            GPU_PROFILE_FRAME("Atmosphere.SkyView")

            auto& lutDescription = graph.GetImageDescription(passData.Lut);
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 1},
				.GroupSize = passData.BindGroup.GetSkyviewLutGroupSize()
            });
        });
}
