#include "rendererpch.h"

#include "CloudCurlNoisePass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/CloudsMapCurlBindGroupRG.generated.h"

Passes::Clouds::CurlNoise::PassData& Passes::Clouds::CurlNoise::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, CloudsMapCurlBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Cloud.CurlNoise.Setup")

            passData.BindGroup = CloudsMapCurlBindGroupRG(graph);

            if (info.CloudCurlNoise.HasValue())
            {
                passData.CloudCurlNoise = graph.Import("CloudCurlNoise.Imported"_hsv,
                    info.CloudCurlNoise, ImageLayout::Undefined);
            }
            else
            {
                const f32 resolution = (f32)*CVars::Get().GetI32CVar("Clouds.CloudCurlNoise.Size"_hsv);
                passData.CloudCurlNoise = graph.Create("CloudCurlNoise"_hsv, RGImageDescription{
                    .Width = resolution,
                    .Height = resolution,
                    .Format = Format::RGBA16_FLOAT
                });
            }

            passData.CloudCurlNoise = passData.BindGroup.SetResourcesCurl(passData.CloudCurlNoise);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Cloud.CurlNoise")
            GPU_PROFILE_FRAME("Cloud.CurlNoise")

            const glm::uvec2 resolution = graph.GetImageDescription(passData.CloudCurlNoise).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudCurlNoiseGroupSize()
            });
        });
}
