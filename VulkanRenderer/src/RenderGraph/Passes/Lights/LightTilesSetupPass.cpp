#include "rendererpch.h"

#include "LightTilesSetupPass.h"

#include "Light/Light.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGCommon.h"
#include "RenderGraph/Passes/Generated/LightTilesSetupBindGroupRG.generated.h"

Passes::LightTilesSetup::PassData& Passes::LightTilesSetup::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, LightTilesSetupBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Setup.Setup")

            passData.BindGroup = LightTilesSetupBindGroupRG(graph);

            auto& globalResources = graph.GetGlobalResources();
            const glm::uvec2 bins = glm::ceil(
                glm::vec2{globalResources.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            passData.Tiles = passData.BindGroup.SetResourcesTiles(graph.Create("Tiles"_hsv, RGBufferDescription{
                .SizeBytes = (u64)(bins.x * bins.y) * sizeof(LightTile)
            }));
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Lights.Tiles.Setup")
            GPU_PROFILE_FRAME("Lights.Tiles.Setup")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            glm::uvec2 bins = glm::ceil(
                glm::vec2{frameContext.Resolution} / glm::vec2{LIGHT_TILE_SIZE_X, LIGHT_TILE_SIZE_Y});
            cmd.Dispatch({
                .Invocations = {bins.x, bins.y, 1},
                .GroupSize = passData.BindGroup.GetSetupTilesGroupSize()
            });
        });
}
