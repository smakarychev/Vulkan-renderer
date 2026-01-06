#include "rendererpch.h"

#include "VPCloudCoveragePass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/CloudsMapCoverageBindGroupRG.generated.h"

Passes::Clouds::VP::Coverage::PassData& Passes::Clouds::VP::Coverage::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, CloudsMapCoverageBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("VP.CoverageMap.Setup")

            passData.BindGroup = CloudsMapCoverageBindGroupRG(graph);

            if (info.CoverageMap.HasValue())
            {
                passData.CoverageMap =
                    graph.Import("CoverageMap.Imported"_hsv, info.CoverageMap, ImageLayout::Undefined);
            }
            else
            {
                const f32 resolution = (f32)*CVars::Get().GetI32CVar("Clouds.CloudMap.Size"_hsv);
                passData.CoverageMap = graph.Create("CoverageMap"_hsv, RGImageDescription{
                    .Width = resolution,
                    .Height = resolution,
                    .Format = Format::R16_FLOAT
                });
            }

            passData.CoverageMap = passData.BindGroup.SetResourcesCoverage(passData.CoverageMap);
            passData.BindGroup.SetResourcesParameters(info.NoiseParameters);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.CoverageMap")
            GPU_PROFILE_FRAME("VP.CoverageMap")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.CoverageMap).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudCoverageNoiseGroupSize()
            });
        });
}
