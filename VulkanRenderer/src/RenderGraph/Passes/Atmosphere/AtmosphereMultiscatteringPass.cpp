#include "rendererpch.h"

#include "AtmosphereMultiscatteringPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/AtmosphereLutMultiscatteringBindGroupRG.generated.h"

namespace RG
{
    enum class ResourceAccessFlags;
}

Passes::Atmosphere::Multiscattering::PassData& Passes::Atmosphere::Multiscattering::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, AtmosphereLutMultiscatteringBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering.Setup")

            passData.BindGroup = AtmosphereLutMultiscatteringBindGroupRG(graph);

            passData.Lut = passData.BindGroup.SetResourcesMultiscatteringLut(graph.Create("Lut"_hsv, RGImageDescription{
                .Width = (f32)*CVars::Get().GetI32CVar("Atmosphere.Multiscattering.Size"_hsv),
                .Height = (f32)*CVars::Get().GetI32CVar("Atmosphere.Multiscattering.Size"_hsv),
                .Format = Format::RGBA16_FLOAT
            }));
            passData.BindGroup.SetResourcesTransmittanceLut(info.TransmittanceLut);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.Multiscattering")
            GPU_PROFILE_FRAME("Atmosphere.Multiscattering")

            auto& lutDescription = graph.GetImageDescription(passData.Lut);

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
				.Invocations = {lutDescription.Width, lutDescription.Height, 64},
				.GroupSize = passData.BindGroup.GetMultiscatteringLutGroupSize()
            });
        });
}
