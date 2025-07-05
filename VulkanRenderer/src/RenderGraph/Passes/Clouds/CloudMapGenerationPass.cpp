#include "CloudMapGenerationPass.h"

#include "CloudsCommon.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudMapBindGroup.generated.h"

Passes::CloudMapGeneration::PassData& Passes::CloudMapGeneration::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("CloudMap.Setup")

            graph.SetShader("cloud-map"_hsv);

            if (info.CloudMap.HasValue())
            {
                passData.CloudMap = graph.Import("CloudMap.Imported"_hsv, info.CloudMap, ImageLayout::Undefined);
            }
            else
            {
                const f32 resolution = (f32)*CVars::Get().GetI32CVar("Clouds.CloudMap.Size"_hsv);
                passData.CloudMap = graph.Create("CloudMap"_hsv, RGImageDescription{
                    .Width = resolution,
                    .Height = resolution,
                    .Format = Format::RGBA16_FLOAT
                });
            }

            passData.CloudMap = graph.WriteImage(passData.CloudMap, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("CloudMap")
            GPU_PROFILE_FRAME("CloudMap")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.CloudMap).Dimensions();

            const Shader& shader = graph.GetShader();
            CloudMapShaderBindGroup bindGroup(shader);
            bindGroup.SetCloudMap(graph.GetImageBinding(passData.CloudMap));

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
