#include "rendererpch.h"

#include "CloudComposePass.h"

#include "RenderGraph/Passes/Generated/CloudsComposeBindGroupRG.generated.h"

Passes::Clouds::Compose::PassData& Passes::Clouds::Compose::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, CloudsComposeBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Cloud.Compose.Setup")

            passData.BindGroup = CloudsComposeBindGroupRG(graph);

            passData.Color = passData.BindGroup.SetResourcesComposed(graph.Create("Composed"_hsv, RGImageDescription{
                .Inference = RGImageInference::Size2d | RGImageInference::Format,
                .Reference = info.SceneColor,
            }));

            passData.BindGroup.SetResourcesSceneColor(info.SceneColor);
            passData.BindGroup.SetResourcesSceneDepth(info.SceneDepth);
            passData.BindGroup.SetResourcesCloudColor(info.CloudColor);
            passData.BindGroup.SetResourcesCloudDepth(info.CloudDepth);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Cloud.Compose")
            GPU_PROFILE_FRAME("Cloud.Compose")
            
            const glm::uvec2 resolution = graph.GetImageDescription(passData.Color).Dimensions();

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {resolution.x, resolution.y, 1},
                .GroupSize = passData.BindGroup.GetCloudCompositionGroupSize()
            });
        });
}
