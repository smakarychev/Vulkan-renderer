#include "LightClustersCompactPass.h"

#include "Light/Light.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Core/Camera.h"
#include "RenderGraph/Passes/Generated/LightClustersCompactBindGroup.generated.h"

namespace
{
    RG::Pass& identifyActiveClusters(StringId name, RG::Graph& renderGraph, RG::Resource clusterVisibility,
        RG::Resource& depth)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Identify.Setup")

                graph.SetShader("light-clusters-compact.shader",
                    ShaderOverrides{
                        ShaderOverride{"IDENTIFY"_hsv, true}});

                passData.ClusterVisibility = graph.Write(clusterVisibility, Compute | Storage);
                passData.Depth = graph.Read(depth, Compute | Sampled);

                graph.UpdateBlackboard(passData);
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
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
                cmd.PushConstants({
                    .PipelineLayout = shader.GetLayout(), 
                    .Data = {pushConstant}});
                cmd.Dispatch({
                    .Invocations = {depthDescription.Width, depthDescription.Height, 1},
                    .GroupSize = {8, 8, 1}});
            });
    }

    RG::Pass& compactActiveClusters(StringId name, RG::Graph& renderGraph, RG::Resource clusters,
        RG::Resource clusterVisibility)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Compact.Setup")

                graph.SetShader("light-clusters-compact.shader",
                    ShaderOverrides{
                        ShaderOverride{"COMPACT"_hsv, true}});

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

                graph.UpdateBlackboard(passData);
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
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
                cmd.Dispatch({
                    .Invocations = {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y * LIGHT_CLUSTER_BINS_Z, 1},
                    .GroupSize = {8, 8, 1}});
            });
    }

    RG::Pass& createIndirectDispatch(StringId name, RG::Graph& renderGraph, RG::Resource clusterCount)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.CreateDispatch.Setup")

                graph.SetShader("light-clusters-compact.shader",
                    ShaderOverrides{
                        ShaderOverride{"CREATE_DISPATCH"_hsv, true}});

                passData.DispatchIndirect = graph.CreateResource("DispatchIndirect"_hsv,
                    GraphBufferDescription{.SizeBytes = sizeof(IndirectDispatchCommand)});

                passData.ActiveClustersCount = graph.Read(clusterCount, Compute | Storage);
                passData.DispatchIndirect = graph.Write(passData.DispatchIndirect, Compute | Storage);

                graph.UpdateBlackboard(passData);
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
                bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
                cmd.Dispatch({
                    .Invocations = {1, 1, 1}});
            });
    }
}

RG::Pass& Passes::LightClustersCompact::addToGraph(StringId name, RG::Graph& renderGraph, RG::Resource clusters,
    RG::Resource clusterVisibility, RG::Resource depth)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& identify = identifyActiveClusters(name.Concatenate(".Identify"), graph, clusterVisibility, depth);
            auto& identifyOutput = graph.GetBlackboard().Get<PassData>(identify);
            auto& compact = compactActiveClusters(name.Concatenate(".Compact"), graph, clusters,
                identifyOutput.ClusterVisibility);
            auto& compactOutput = graph.GetBlackboard().Get<PassData>(compact);
            auto& creatDispatch = createIndirectDispatch(name.Concatenate(".CreateDispatch"), graph,
                compactOutput.ActiveClustersCount);
            auto& createDispatchOutput = graph.GetBlackboard().Get<PassData>(creatDispatch);
            compactOutput.Depth = depth;
            compactOutput.DispatchIndirect = createDispatchOutput.DispatchIndirect;
            compactOutput.ActiveClustersCount = createDispatchOutput.ActiveClustersCount;
            
            graph.UpdateBlackboard(compactOutput);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
