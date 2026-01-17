#include "rendererpch.h"

#include "LightClustersBinPass.h"

#include "RenderGraph/Passes/Generated/LightClustersBinBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/LightClustersCompactBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/LightClustersCompactCreateDispatchBindGroupRG.generated.h"
#include "RenderGraph/Passes/Generated/LightClustersCompactIdentifyBindGroupRG.generated.h"
#include "Scene/SceneLight.h"

namespace
{
struct PassDataIdentify
{
    RG::Resource Visibility{};
};
struct PassDataCompactPrepare
{
    RG::Resource ActiveClusters{};
    RG::Resource ActiveClustersCount{};
};
struct PassDataCrateDispatch
{
    RG::Resource DispatchIndirect{};
};
struct PassDataCompact
{
    RG::Resource ActiveClusters{};
    RG::Resource ActiveClustersCount{};
    RG::Resource DispatchIndirect{};
};

PassDataIdentify& identifyActiveClusters(StringId name, RG::Graph& renderGraph,
    RG::Resource clusterVisibility, RG::Resource depth, RG::Resource view)
{
    using namespace RG;
    using PassData = PassDataWithBind<PassDataIdentify, LightClustersCompactIdentifyBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Identify.Setup")

            passData.BindGroup = LightClustersCompactIdentifyBindGroupRG(graph);

            passData.Visibility = passData.BindGroup.SetResourcesVisibility(clusterVisibility);
            passData.BindGroup.SetResourcesDepth(depth);
            passData.BindGroup.SetResourcesView(view);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Identify")
            GPU_PROFILE_FRAME("Lights.Clusters.Identify")

            auto& depthDescription = graph.GetImageDescription(depth);

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {depthDescription.Width, depthDescription.Height, 1},
                .GroupSize = passData.BindGroup.GetCompactIdentifyClustersGroupSize()
            });
        });
}

PassDataCompactPrepare& compactActiveClustersPrepare(StringId name, RG::Graph& renderGraph, RG::Resource clusterVisibility)
{
    using namespace RG;
    using PassData = PassDataWithBind<PassDataCompactPrepare, LightClustersCompactBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Compact.Setup")

            passData.BindGroup = LightClustersCompactBindGroupRG(graph);
            
            passData.ActiveClusters = passData.BindGroup.SetResourcesActiveClusters(
                graph.Create("Clusters.Active"_hsv, RGBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u16)}));
            passData.ActiveClustersCount = passData.BindGroup.SetResourcesActiveClustersCount(
                graph.Create("Clusters.ActiveCount"_hsv, RGBufferDescription{.SizeBytes = sizeof(u32)}));
            passData.ActiveClustersCount = graph.Upload(passData.ActiveClustersCount, 0);
            
            passData.BindGroup.SetResourcesVisibility(clusterVisibility);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Compact")
            GPU_PROFILE_FRAME("Lights.Clusters.Compact")
            
            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y * LIGHT_CLUSTER_BINS_Z, 1},
                .GroupSize = passData.BindGroup.GetCompactClustersGroupSize()
            });
        });
}

PassDataCrateDispatch& createIndirectDispatch(StringId name, RG::Graph& renderGraph, RG::Resource clusterCount)
{
    using namespace RG;
    using PassData = PassDataWithBind<PassDataCrateDispatch, LightClustersCompactCreateDispatchBindGroupRG>;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch.Setup")

            passData.BindGroup = LightClustersCompactCreateDispatchBindGroupRG(graph);

            passData.DispatchIndirect = passData.BindGroup.SetResourcesCommand(graph.Create("DispatchIndirect"_hsv,
                RGBufferDescription{.SizeBytes = sizeof(IndirectDispatchCommand)}));

            passData.BindGroup.SetResourcesActiveClustersCount(clusterCount);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph&)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch")
            GPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.Dispatch({
                .Invocations = {1, 1, 1}
            });
        });
}

PassDataCompact& compactActiveClusters(StringId name, RG::Graph& renderGraph,
    const Passes::LightClustersBin::ExecutionInfo& info)
{
    using namespace RG;

    return renderGraph.AddRenderPass<PassDataCompact>(name,
        [&](Graph& graph, PassDataCompact& passData)
        {
            auto& identify = identifyActiveClusters(name.Concatenate(".Identify"), graph,
                info.ClusterVisibility, info.Depth, info.ViewInfo);
            auto& compact = compactActiveClustersPrepare(name.Concatenate(".Prepare"), graph, identify.Visibility);
            auto& createDispatch = createIndirectDispatch(name.Concatenate(".CreateDispatch"), graph,
                compact.ActiveClustersCount);

            passData.ActiveClusters = compact.ActiveClusters;
            passData.ActiveClustersCount = compact.ActiveClustersCount;
            passData.DispatchIndirect = createDispatch.DispatchIndirect;
        },
        [=](const PassDataCompact&, FrameContext&, const Graph&)
        {
        });
}
}

Passes::LightClustersBin::PassData& Passes::LightClustersBin::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataBind : PassDataWithBind<PassData, LightClustersBinBindGroupRG>
    {
        Resource DispatchIndirect{};
    };
    
    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin.Setup")

            auto& compact = compactActiveClusters(name.Concatenate(".Compact"), graph, info);

            passData.BindGroup = LightClustersBinBindGroupRG(graph);

            passData.Clusters = passData.BindGroup.SetResourcesClusters(info.Clusters);
            passData.DispatchIndirect = graph.ReadBuffer(compact.DispatchIndirect, Indirect);

            passData.BindGroup.SetResourcesActiveClusters(compact.ActiveClusters);
            passData.BindGroup.SetResourcesActiveClustersCount(compact.ActiveClustersCount);
            passData.BindGroup.SetResourcesView(info.ViewInfo);
            passData.BindGroup.SetResourcesPointLights(
                graph.Import("Light.PointLights"_hsv, info.Light->GetBuffers().PointLights));
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin")
            GPU_PROFILE_FRAME("Lights.Clusters.Bin")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd);
            cmd.DispatchIndirect({
                .Buffer = graph.GetBuffer(passData.DispatchIndirect)
            });
        });
}
