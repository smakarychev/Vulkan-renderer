#include "rendererpch.h"

#include "VPCloudProfileMapPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/CloudsMapProfileBindGroupRG.generated.h"

Passes::Clouds::VP::ProfileMap::PassData& Passes::Clouds::VP::ProfileMap::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, CloudsMapProfileBindGroupRG>;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("VP.ProfileMap.Setup")

            passData.BindGroup = CloudsMapProfileBindGroupRG(graph);

            if (info.ProfileMap.HasValue())
            {
                passData.ProfileMap =
                    graph.Import("ProfileMap.Imported"_hsv, info.ProfileMap, ImageLayout::Undefined);
            }
            else
            {
                const f32 resolution = (f32)*CVars::Get().GetI32CVar("Clouds.CloudMap.Size"_hsv);
                passData.ProfileMap = graph.Create("ProfileMap"_hsv, RGImageDescription{
                    .Width = resolution,
                    .Height = resolution,
                    .Format = Format::RGBA16_FLOAT
                });
            }

            passData.ProfileMap = passData.BindGroup.SetResourcesProfile(passData.ProfileMap);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.ProfileMap")
            GPU_PROFILE_FRAME("VP.ProfileMap")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.ProfileMap).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudProfileNoiseGroupSize()
            });
        });
}
