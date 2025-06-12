#include "LightClustersCompactPass.h"

#include "Light/Light.h"
#include "RenderGraph/RGGraph.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Core/Camera.h"
#include "RenderGraph/Passes/Generated/LightClustersCompactBindGroup.generated.h"

namespace
{
    Passes::LightClustersCompact::PassData& identifyActiveClusters(StringId name, RG::Graph& renderGraph,
        RG::Resource clusterVisibility, RG::Resource depth)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Identify.Setup")

                graph.SetShader("light-clusters-compact"_hsv,
                    ShaderSpecializations{
                        ShaderSpecialization{"IDENTIFY"_hsv, true}});

                passData.ClusterVisibility = graph.WriteBuffer(clusterVisibility, Compute | Storage);
                passData.Depth = graph.ReadImage(depth, Compute | Sampled);
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Identify")
                GPU_PROFILE_FRAME("Lights.Clusters.Identify")

                auto& depthDescription = graph.GetImageDescription(depth);

                const Shader& shader = graph.GetShader();
                LightClustersCompactShaderBindGroup bindGroup(shader);
                bindGroup.SetDepth(graph.GetImageBinding(passData.Depth));
                bindGroup.SetClusterVisibility(graph.GetBufferBinding(passData.ClusterVisibility));

                struct PushConstant
                {
                    f32 Near;
                    f32 Far;
                };
                PushConstant pushConstant = {
                    .Near = frameContext.PrimaryCamera->GetNear(),
                    .Far = frameContext.PrimaryCamera->GetFar()};

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});
                cmd.Dispatch({
                    .Invocations = {depthDescription.Width, depthDescription.Height, 1},
                    .GroupSize = {8, 8, 1}});
            });
    }

    Passes::LightClustersCompact::PassData& compactActiveClusters(StringId name, RG::Graph& renderGraph,
        RG::Resource clusters, RG::Resource clusterVisibility)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Compact.Setup")

                graph.SetShader("light-clusters-compact"_hsv,
                    ShaderSpecializations{
                        ShaderSpecialization{"COMPACT"_hsv, true}});

                passData.ActiveClusters = graph.Create("Clusters.Active"_hsv,
                    RGBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u16)});
                passData.ActiveClustersCount = graph.Create("Clusters.ActiveCount"_hsv,
                    RGBufferDescription{.SizeBytes = sizeof(u32)});

                passData.Clusters = graph.ReadBuffer(clusters, Compute | Storage);
                passData.ClusterVisibility = graph.ReadWriteBuffer(clusterVisibility, Compute | Storage);
                passData.ActiveClusters = graph.WriteBuffer(passData.ActiveClusters, Compute | Storage);
                passData.ActiveClustersCount = graph.ReadWriteBuffer(passData.ActiveClustersCount, Compute | Storage);
                passData.ActiveClustersCount = graph.Upload(passData.ActiveClustersCount, 0);
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Compact")
                GPU_PROFILE_FRAME("Lights.Clusters.Compact")

                const Shader& shader = graph.GetShader();
                LightClustersCompactShaderBindGroup bindGroup(shader);
                
                bindGroup.SetClusters(graph.GetBufferBinding(passData.Clusters));
                bindGroup.SetClusterVisibility(graph.GetBufferBinding(passData.ClusterVisibility));
                bindGroup.SetActiveClusters(graph.GetBufferBinding(passData.ActiveClusters));
                bindGroup.SetCount(graph.GetBufferBinding(passData.ActiveClustersCount));

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
                cmd.Dispatch({
                    .Invocations = {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y * LIGHT_CLUSTER_BINS_Z, 1},
                    .GroupSize = {8, 8, 1}});
            });
    }

    Passes::LightClustersCompact::PassData& createIndirectDispatch(StringId name, RG::Graph& renderGraph,
        RG::Resource clusterCount)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch.Setup")

                graph.SetShader("light-clusters-compact"_hsv,
                    ShaderSpecializations{
                        ShaderSpecialization{"CREATE_DISPATCH"_hsv, true}});

                passData.DispatchIndirect = graph.Create("DispatchIndirect"_hsv,
                    RGBufferDescription{.SizeBytes = sizeof(IndirectDispatchCommand)});

                passData.ActiveClustersCount = graph.ReadBuffer(clusterCount, Compute | Storage);
                passData.DispatchIndirect = graph.WriteBuffer(passData.DispatchIndirect, Compute | Storage);
            },
            [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch")
                GPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch")

                const Shader& shader = graph.GetShader();
                LightClustersCompactShaderBindGroup bindGroup(shader);
                
                bindGroup.SetCount(graph.GetBufferBinding(passData.ActiveClustersCount));
                bindGroup.SetIndirectDispatch(graph.GetBufferBinding(passData.DispatchIndirect));

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
                cmd.Dispatch({
                    .Invocations = {1, 1, 1}});
            });
    }
}

Passes::LightClustersCompact::PassData& Passes::LightClustersCompact::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& identify = identifyActiveClusters(name.Concatenate(".Identify"), graph,
                info.ClusterVisibility, info.Depth);
            auto& compact = compactActiveClusters(name.Concatenate(".Compact"), graph, info.Clusters,
                identify.ClusterVisibility);
            auto& createDispatch = createIndirectDispatch(name.Concatenate(".CreateDispatch"), graph,
                compact.ActiveClustersCount);

            passData = compact;
            passData.Depth = info.Depth;
            passData.DispatchIndirect = createDispatch.DispatchIndirect;
            passData.ActiveClustersCount = createDispatch.ActiveClustersCount;
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
        });
}
