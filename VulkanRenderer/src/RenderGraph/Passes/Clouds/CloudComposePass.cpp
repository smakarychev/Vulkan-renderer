#include "rendererpch.h"

#include "CloudComposePass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/CloudComposeBindGroup.generated.h"

Passes::Clouds::Compose::PassData& Passes::Clouds::Compose::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Cloud.Compose.Setup")

            graph.SetShader("cloud-compose"_hsv);

            passData.ColorOut = graph.Create("Composed"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d | RGImageInference::Format,
                .Reference = info.SceneColor,
            });

            passData.SceneColor = graph.ReadImage(info.SceneColor, Compute | Sampled);
            passData.SceneDepth = graph.ReadImage(info.SceneDepth, Compute | Sampled);
            passData.CloudColor = graph.ReadImage(info.CloudColor, Compute | Sampled);
            passData.CloudDepth = graph.ReadImage(info.CloudDepth, Compute | Sampled);
            passData.ColorOut = graph.WriteImage(passData.ColorOut, Compute | Storage);
            passData.ViewInfo = graph.ReadBuffer(info.ViewInfo, Compute | Uniform);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Cloud.Compose")
            GPU_PROFILE_FRAME("Cloud.Compose")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.ColorOut).Dimensions();

            const Shader& shader = graph.GetShader();
            CloudComposeShaderBindGroup bindGroup(shader);
            bindGroup.SetSceneColor(graph.GetImageBinding(passData.SceneColor));
            bindGroup.SetSceneDepth(graph.GetImageBinding(passData.SceneDepth));
            bindGroup.SetClouds(graph.GetImageBinding(passData.CloudColor));
            bindGroup.SetCloudsDepth(graph.GetImageBinding(passData.CloudDepth));
            bindGroup.SetCloudsComposed(graph.GetImageBinding(passData.ColorOut));
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.ViewInfo));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = {8, 8, 1}
            });
        });
}
