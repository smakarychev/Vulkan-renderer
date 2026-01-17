#include "rendererpch.h"
#include "AtmosphereTransmittanceAtViewPass.h"

#include "RenderGraph/Passes/Generated/AtmosphereLutTransmittanceAtViewBindGroupRG.generated.h"

Passes::AtmosphereLutTransmittanceAtView::PassData& Passes::AtmosphereLutTransmittanceAtView::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, AtmosphereLutTransmittanceAtViewBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.TransmittanceLutAtView.Setup")

            passData.BindGroup = AtmosphereLutTransmittanceAtViewBindGroupRG(graph);

            passData.ViewInfo = passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesTransmittanceLut(info.TransmittanceLut);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Atmosphere.TransmittanceLutAtView")
            GPU_PROFILE_FRAME("Atmosphere.TransmittanceLutAtView")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {1, 1, 1}
            });
        });
}
