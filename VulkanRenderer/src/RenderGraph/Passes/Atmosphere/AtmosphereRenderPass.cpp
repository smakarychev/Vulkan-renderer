#include "rendererpch.h"

#include "AtmosphereRenderPass.h"

#include "RenderGraph/Passes/Generated/AtmosphereRenderBindGroupRG.generated.h"

Passes::Atmosphere::Render::PassData& Passes::Atmosphere::Render::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, AtmosphereRenderBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Atmosphere.Render.Setup")

            passData.BindGroup = AtmosphereRenderBindGroupRG(graph, info.IsPrimaryView ?
                AtmosphereRenderBindGroupRG::Variants::PrimaryView :
                AtmosphereRenderBindGroupRG::Variants::EnvironmentView);

            passData.Color = graph.RenderTarget(info.ColorIn, {});

            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesSkyviewLut(info.SkyViewLut);
            if (info.IsPrimaryView)
            {
                passData.BindGroup.SetResourcesAerialPerspectiveLut(info.AerialPerspective);
                passData.BindGroup.SetResourcesDepth(info.DepthIn);
            }
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Atmosphere.Render")
            GPU_PROFILE_FRAME("Atmosphere.Render")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.Draw({.VertexCount = 3});
        });
}
