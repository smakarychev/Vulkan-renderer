#include "rendererpch.h"

#include "AtmosphereTransmittanceLutPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/AtmosphereLutTransmittanceBindGroupRG.generated.h"

Passes::Atmosphere::Transmittance::PassData& Passes::Atmosphere::Transmittance::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, AtmosphereLutTransmittanceBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Transmittance.Setup")

            passData.BindGroup = AtmosphereLutTransmittanceBindGroupRG(graph);

            passData.Lut = passData.BindGroup.SetResourcesLut(graph.Create("Lut"_hsv, RGImageDescription{
                .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.Transmittance.Width"_hsv),
                .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.Transmittance.Height"_hsv),
                .Format = Format::RGBA16_FLOAT
            }));

            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.Transmittance")
            GPU_PROFILE_FRAME("Atmosphere.Transmittance")

            auto& lutDescription = graph.GetImageDescription(passData.Lut);

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 1},
				.GroupSize = passData.BindGroup.GetTransmittanceLutGroupSize()
            });
        });
}
