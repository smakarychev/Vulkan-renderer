#include "rendererpch.h"

#include "LightTilesBinPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/LightTilesBinBindGroupRG.generated.h"
#include "RenderGraph/Passes/Utility/ImGuiTexturePass.h"
#include "Scene/SceneLight.h"

Passes::LightTilesBin::PassData& Passes::LightTilesBin::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, LightTilesBinBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Bin.Setup")

            passData.BindGroup = LightTilesBinBindGroupRG(graph);

            passData.Tiles = passData.BindGroup.SetResourcesTiles(info.Tiles);
            passData.BindGroup.SetResourcesDepth(info.Depth);
            passData.BindGroup.SetResourcesPointLights(
                graph.Import("Light.PointLights"_hsv, info.Light->GetBuffers().PointLights));
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=, description = renderGraph.GetImageDescription(info.Depth)]
        (const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Bin")
            GPU_PROFILE_FRAME("Lights.Tiles.Bin")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {description.Width, description.Height, 1},
                .GroupSize = {LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y, 1}
            });
        });
}
