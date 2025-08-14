#include "VPCloudCoveragePass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Clouds/CloudCommon.h"
#include "RenderGraph/Passes/Generated/CloudVpCoverageBindGroup.generated.h"

Passes::Clouds::VP::Coverage::PassData& Passes::Clouds::VP::Coverage::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("VP.CoverageMap.Setup")

            graph.SetShader("cloud-vp-coverage"_hsv);

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

            passData.CoverageMap = graph.WriteImage(passData.CoverageMap, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("VP.CoverageMap")
            GPU_PROFILE_FRAME("VP.CoverageMap")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.CoverageMap).Dimensions();

            const Shader& shader = graph.GetShader();
            CloudVpCoverageShaderBindGroup bindGroup(shader);
            bindGroup.SetCoverageMap(graph.GetImageBinding(passData.CoverageMap));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(),
                .Data = {*info.NoiseParameters}
            });
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = {8, 8, 1}
            });
        });
}
