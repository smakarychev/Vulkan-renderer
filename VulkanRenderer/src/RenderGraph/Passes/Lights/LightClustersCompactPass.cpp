#include "LightClustersCompactPass.h"

#include "Light/Light.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Core/Camera.h"
#include "Vulkan/RenderCommand.h"

namespace
{
    RG::Pass& identifyActiveClusters(std::string_view name, RG::Graph& renderGraph, RG::Resource clusterVisibility,
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
                        ShaderOverride{{"IDENTIFY"}, true}});

                passData.ClusterVisibility = graph.Write(clusterVisibility, Compute | Storage);
                passData.Depth = graph.Read(depth, Compute | Sampled);

                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Identify")
                GPU_PROFILE_FRAME("Lights.Clusters.Identify")

                const Shader& shader = resources.GetGraph()->GetShader();
                auto pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

                const Texture& depthTexture = resources.GetTexture(depth);
                
                resourceDescriptors.UpdateBinding("u_depth", depthTexture.BindingInfo(
                    ImageFilter::Linear, ImageLayout::Readonly));
                resourceDescriptors.UpdateBinding("u_cluster_visibility", resources.GetBuffer(
                    passData.ClusterVisibility).BindingInfo());

                struct PushConstant
                {
                    f32 Near;
                    f32 Far;
                };
                PushConstant pushConstant = {
                    .Near = frameContext.PrimaryCamera->GetNear(),
                    .Far = frameContext.PrimaryCamera->GetFar()};

                auto& cmd = frameContext.Cmd;
                samplerDescriptors.BindComputeImmutableSamplers(cmd, shader.GetLayout());
                RenderCommand::BindCompute(cmd, pipeline);
                RenderCommand::PushConstants(cmd, shader.GetLayout(), pushConstant);
                resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

                RenderCommand::Dispatch(cmd,
                    {depthTexture.Description().Width, depthTexture.Description().Height, 1},
                    {8, 8, 1});
            });
    }

    RG::Pass& compactActiveClusters(std::string_view name, RG::Graph& renderGraph, RG::Resource clusters,
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
                        ShaderOverride{{"COMPACT"}, true}});

                passData.ActiveClusters = graph.CreateResource(std::format("{}.Clusters.Active", name),
                    GraphBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u16)});
                passData.ActiveClustersCount = graph.CreateResource(std::format("{}.Clusters.ActiveCount", name),
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
                auto pipeline = shader.Pipeline(); 
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

                resourceDescriptors.UpdateBinding("u_clusters", resources.GetBuffer(passData.Clusters).BindingInfo());
                resourceDescriptors.UpdateBinding("u_cluster_visibility", resources.GetBuffer(
                    passData.ClusterVisibility).BindingInfo());
                resourceDescriptors.UpdateBinding("u_active_clusters", resources.GetBuffer(
                    passData.ActiveClusters).BindingInfo());
                resourceDescriptors.UpdateBinding("u_count", resources.GetBuffer(
                    passData.ActiveClustersCount).BindingInfo());

                auto& cmd = frameContext.Cmd;
                RenderCommand::BindCompute(cmd, pipeline);
                resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

                RenderCommand::Dispatch(cmd,
                    {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y * LIGHT_CLUSTER_BINS_Z, 1},
                    {8, 8, 1});
            });
    }

    RG::Pass& createIndirectDispatch(std::string_view name, RG::Graph& renderGraph, RG::Resource clusterCount)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.CreateDisptach.Setup")

                graph.SetShader("light-clusters-compact.shader",
                    ShaderOverrides{
                        ShaderOverride{{"CREATE_DISPATCH"}, true}});

                passData.DispatchIndirect = graph.CreateResource(std::format("{}.DispatchIndirect", name),
                    GraphBufferDescription{.SizeBytes = sizeof(IndirectDispatchCommand)});

                passData.ActiveClustersCount = graph.Read(clusterCount, Compute | Storage);
                passData.DispatchIndirect = graph.Write(passData.DispatchIndirect, Compute | Storage);

                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.CreateDisptach")
                GPU_PROFILE_FRAME("Lights.Clusters.CreateDisptach")

                const Shader& shader = resources.GetGraph()->GetShader();
                auto pipeline = shader.Pipeline(); 
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

                resourceDescriptors.UpdateBinding("u_count", resources.GetBuffer(
                    passData.ActiveClustersCount).BindingInfo());
                resourceDescriptors.UpdateBinding("u_indirect_dispatch", resources.GetBuffer(
                    passData.DispatchIndirect).BindingInfo());

                auto& cmd = frameContext.Cmd;
                RenderCommand::BindCompute(cmd, pipeline);
                resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());

                RenderCommand::Dispatch(cmd, {1, 1, 1});
            });
    }
}

RG::Pass& Passes::LightClustersCompact::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource clusters,
    RG::Resource clusterVisibility, RG::Resource depth)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& identify = identifyActiveClusters(std::format("{}.Identify", name), graph, clusterVisibility, depth);
            auto& identifyOutput = graph.GetBlackboard().Get<PassData>(identify);
            auto& compact = compactActiveClusters(std::format("{}.Compact", name), graph, clusters,
                identifyOutput.ClusterVisibility);
            auto& compactOutput = graph.GetBlackboard().Get<PassData>(compact);
            auto& creatDispatch = createIndirectDispatch(std::format("{}.CreateDispatch", name), graph,
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
