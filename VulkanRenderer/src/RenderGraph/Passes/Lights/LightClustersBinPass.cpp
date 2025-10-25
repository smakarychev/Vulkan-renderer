#include "rendererpch.h"

#include "LightClustersBinPass.h"

#include "Core/Camera.h"
#include "RenderGraph/RGGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/LightClustersBinBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

Passes::LightClustersBin::PassData& Passes::LightClustersBin::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource Dispatch{};
        Resource ActiveClusters{};
        Resource ClusterCount{};
        SceneLightResources SceneLightResources{};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin.Setup")

            graph.SetShader("light-clusters-bin"_hsv);

            passData.Dispatch = graph.ReadBuffer(info.DispatchIndirect, Indirect);
            passData.Clusters = graph.ReadWriteBuffer(info.Clusters, Compute | Storage);
            passData.ActiveClusters = graph.ReadBuffer(info.ActiveClusters, Compute | Storage);
            passData.ClusterCount = graph.ReadBuffer(info.ClustersCount, Compute | Storage);
            passData.SceneLightResources = RgUtils::readSceneLight(*info.Light, graph, Compute);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin")
            GPU_PROFILE_FRAME("Lights.Clusters.Bin")

            const Shader& shader = graph.GetShader();
            LightClustersBinShaderBindGroup bindGroup(shader);
            bindGroup.SetClusters(graph.GetBufferBinding(passData.Clusters));
            bindGroup.SetActiveClusters(graph.GetBufferBinding(passData.ActiveClusters));
            bindGroup.SetCount(graph.GetBufferBinding(passData.ClusterCount));
            bindGroup.SetPointLights(graph.GetBufferBinding(passData.SceneLightResources.PointLights));
            bindGroup.SetLightsInfo(graph.GetBufferBinding(passData.SceneLightResources.LightsInfo));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {frameContext.PrimaryCamera->GetView()}});
            cmd.DispatchIndirect({
                .Buffer = graph.GetBuffer(passData.Dispatch)});
        });
}
