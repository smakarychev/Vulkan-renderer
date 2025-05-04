#include "LightClustersCompactPass.h"

#include "Light/Light.h"
#include "RenderGraph/RenderGraph.h"
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

                passData.ClusterVisibility = graph.Write(clusterVisibility, Compute | Storage);
                passData.Depth = graph.Read(depth, Compute | Sampled);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Identify")
                GPU_PROFILE_FRAME("Lights.Clusters.Identify")

                auto&& [depthTexture, depthDescription] = resources.GetTextureWithDescription(depth);

                const Shader& shader = resources.GetGraph()->GetShader();
                LightClustersCompactShaderBindGroup bindGroup(shader);
                bindGroup.SetDepth({.Image = depthTexture}, ImageLayout::Readonly);
                bindGroup.SetClusterVisibility({.Buffer = resources.GetBuffer(passData.ClusterVisibility)});

                struct PushConstant
                {
                    f32 Near;
                    f32 Far;
                };
                PushConstant pushConstant = {
                    .Near = frameContext.PrimaryCamera->GetNear(),
                    .Far = frameContext.PrimaryCamera->GetFar()};

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});
                cmd.Dispatch({
                    .Invocations = {depthDescription.Width, depthDescription.Height, 1},
                    .GroupSize = {8, 8, 1}});
            }).Data;
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

                passData.ActiveClusters = graph.CreateResource("Clusters.Active"_hsv,
                    GraphBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u16)});
                passData.ActiveClustersCount = graph.CreateResource("Clusters.ActiveCount"_hsv,
                    GraphBufferDescription{.SizeBytes = sizeof(u32)});

                passData.Clusters = graph.Read(clusters, Compute | Storage);
                passData.ClusterVisibility = graph.Read(clusterVisibility, Compute | Storage);
                passData.ClusterVisibility = graph.Write(clusterVisibility, Compute | Storage);
                passData.ActiveClusters = graph.Write(passData.ActiveClusters, Compute | Storage);
                passData.ActiveClustersCount = graph.Read(passData.ActiveClustersCount, Compute | Storage);
                passData.ActiveClustersCount = graph.Write(passData.ActiveClustersCount, Compute | Storage);
                graph.Upload(passData.ActiveClustersCount, 0);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Compact")
                GPU_PROFILE_FRAME("Lights.Clusters.Compact")

                const Shader& shader = resources.GetGraph()->GetShader();
                LightClustersCompactShaderBindGroup bindGroup(shader);
                
                bindGroup.SetClusters({.Buffer = resources.GetBuffer(passData.Clusters)});
                bindGroup.SetClusterVisibility({.Buffer = resources.GetBuffer(passData.ClusterVisibility)});
                bindGroup.SetActiveClusters({.Buffer = resources.GetBuffer(passData.ActiveClusters)});
                bindGroup.SetCount({.Buffer = resources.GetBuffer(passData.ActiveClustersCount)});

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
                cmd.Dispatch({
                    .Invocations = {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y * LIGHT_CLUSTER_BINS_Z, 1},
                    .GroupSize = {8, 8, 1}});
            }).Data;
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

                passData.DispatchIndirect = graph.CreateResource("DispatchIndirect"_hsv,
                    GraphBufferDescription{.SizeBytes = sizeof(IndirectDispatchCommand)});

                passData.ActiveClustersCount = graph.Read(clusterCount, Compute | Storage);
                passData.DispatchIndirect = graph.Write(passData.DispatchIndirect, Compute | Storage);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch")
                GPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch")

                const Shader& shader = resources.GetGraph()->GetShader();
                LightClustersCompactShaderBindGroup bindGroup(shader);
                
                bindGroup.SetCount({.Buffer = resources.GetBuffer(passData.ActiveClustersCount)});
                bindGroup.SetIndirectDispatch({.Buffer = resources.GetBuffer(passData.DispatchIndirect)});

                auto& cmd = frameContext.CommandList;
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetFrameAllocators());
                cmd.Dispatch({
                    .Invocations = {1, 1, 1}});
            }).Data;
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
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        }).Data;
}
