#include "rendererpch.h"

#include "VPCloudProfileMapPass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudVpProfileBindGroup.generated.h"

Passes::Clouds::VP::ProfileMap::PassData& Passes::Clouds::VP::ProfileMap::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("VP.ProfileMap.Setup")

            graph.SetShader("cloud-vp-profile"_hsv);

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

            passData.ProfileMap = graph.WriteImage(passData.ProfileMap, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.ProfileMap")
            GPU_PROFILE_FRAME("VP.ProfileMap")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.ProfileMap).Dimensions();

            const Shader& shader = graph.GetShader();
            CloudVpProfileShaderBindGroup bindGroup(shader);
            bindGroup.SetProfileMap(graph.GetImageBinding(passData.ProfileMap));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = {8, 8, 1}
            });
        });
}
