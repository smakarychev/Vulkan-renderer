#include "rendererpch.h"

#include "CloudCurlNoisePass.h"

#include "cvars/CVarSystem.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudCurlBindGroup.generated.h"

Passes::Clouds::CurlNoise::PassData& Passes::Clouds::CurlNoise::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Cloud.CurlNoise.Setup")

            graph.SetShader("cloud-curl"_hsv);

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

            passData.CloudCurlNoise = graph.WriteImage(passData.CloudCurlNoise, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Cloud.CurlNoise")
            GPU_PROFILE_FRAME("Cloud.CurlNoise")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.CloudCurlNoise).Dimensions();

            const Shader& shader = graph.GetShader();
            CloudCurlShaderBindGroup bindGroup(shader);
            bindGroup.SetCurlNoise(graph.GetImageBinding(passData.CloudCurlNoise));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = {8, 8, 1}
            });
        });
}
