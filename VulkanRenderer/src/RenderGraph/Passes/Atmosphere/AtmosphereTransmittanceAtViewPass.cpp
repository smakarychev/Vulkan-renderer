#include "rendererpch.h"
#include "AtmosphereTransmittanceAtViewPass.h"

#include "RenderGraph/Passes/Generated/AtmosphereUpdateSunParametersBindGroupRG.generated.h"

Passes::AtmosphereUpdateSunParameters::PassData& Passes::AtmosphereUpdateSunParameters::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, AtmosphereUpdateSunParametersBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.AtmosphereUpdateSunParameters.Setup")

            passData.BindGroup = AtmosphereUpdateSunParametersBindGroupRG(graph);

            passData.ViewInfo = passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesTransmittanceLut(info.TransmittanceLut);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Atmosphere.AtmosphereUpdateSunParameters")
            GPU_PROFILE_FRAME("Atmosphere.AtmosphereUpdateSunParameters")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {1, 1, 1}
            });
        });
}
