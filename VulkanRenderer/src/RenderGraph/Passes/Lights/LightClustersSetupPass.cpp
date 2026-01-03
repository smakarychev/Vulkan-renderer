#include "rendererpch.h"

#include "LightClustersSetupPass.h"

#include "Light/Light.h"
#include "RenderGraph/Passes/Generated/LightClustersSetupBindGroupRG.generated.h"

Passes::LightClustersSetup::PassData& Passes::LightClustersSetup::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, LightClustersSetupBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Setup.Setup")

            passData.BindGroup = LightClustersSetupBindGroupRG(graph);

            passData.Clusters = passData.BindGroup.SetResourcesClusters(graph.Create("Clusters"_hsv,
                RGBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(LightCluster)}));
            passData.ClusterVisibility = passData.BindGroup.SetResourcesVisibility(graph.Create("Cluster.Visibility"_hsv,
                RGBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u8)}));
            passData.BindGroup.SetResourcesView(info.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Setup")
            GPU_PROFILE_FRAME("Lights.Clusters.Setup")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.Dispatch({
                .Invocations = {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y, LIGHT_CLUSTER_BINS_Z},
                .GroupSize = passData.BindGroup.GetSetupClustersGroupSize()
            });
        });
}
