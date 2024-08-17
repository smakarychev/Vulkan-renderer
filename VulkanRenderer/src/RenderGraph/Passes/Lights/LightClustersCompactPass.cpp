#include "LightClustersCompactPass.h"

#include "Light/Light.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Core/Camera.h"
#include "Vulkan/RenderCommand.h"

namespace
{
    RG::Pass& identifyActiveClusters(std::string_view name, RG::Graph& renderGraph, RG::Resource clusters,
        RG::Resource& depth)
    {
        using namespace RG;
        using enum ResourceAccessFlags;
        using PassData = Passes::LightClustersCompact::PassData;
        
        return renderGraph.AddRenderPass<PassData>(name,
            [&](Graph& graph, PassData& passData)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Identify.Setup")

                graph.SetShader("../assets/shaders/light-clusters-compact.shader",
                    ShaderOverrides{}
                        .Add({"IDENTIFY"}, true));

                passData.ClustersVisibility = graph.CreateResource(std::format("{}.Clusters.Visibility", name),
                    GraphBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u32)});

                passData.Clusters = graph.Read(clusters, Compute | Storage);
                passData.ClustersVisibility = graph.Write(passData.ClustersVisibility, Compute | Storage);
                passData.Depth = graph.Read(depth, Compute | Sampled);

                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Identify")
                GPU_PROFILE_FRAME("Lights.Clusters.Identify")

                const Shader& shader = resources.GetGraph()->GetShader();
                auto& pipeline = shader.Pipeline(); 
                auto& samplerDescriptors = shader.Descriptors(ShaderDescriptorsKind::Sampler);
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

                const Texture& depthTexture = resources.GetTexture(depth);
                
                resourceDescriptors.UpdateBinding("u_depth", depthTexture.BindingInfo(
                    ImageFilter::Linear, ImageLayout::Readonly));
                resourceDescriptors.UpdateBinding("u_clusters", resources.GetBuffer(passData.Clusters).BindingInfo());
                resourceDescriptors.UpdateBinding("u_cluster_visibility", resources.GetBuffer(
                    passData.ClustersVisibility).BindingInfo());

                struct PushConstant
                {
                    f32 Near;
                    f32 Far;
                };
                PushConstant pushConstant = {
                    .Near = frameContext.PrimaryCamera->GetNear(),
                    .Far = frameContext.PrimaryCamera->GetFar()};

                auto& cmd = frameContext.Cmd;
                samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
                pipeline.BindCompute(cmd);
                RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);
                resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

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

                graph.SetShader("../assets/shaders/light-clusters-compact.shader",
                    ShaderOverrides{}
                        .Add({"COMPACT"}, true));

                passData.ActiveClusters = graph.CreateResource(std::format("{}.Clusters.Active", name),
                    GraphBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(LightCluster)});
                passData.ActiveClustersCount = graph.CreateResource(std::format("{}.Clusters.ActiveCount", name),
                    GraphBufferDescription{.SizeBytes = sizeof(u32)});

                passData.Clusters = graph.Read(clusters, Compute | Storage);
                passData.ClustersVisibility = graph.Read(clusterVisibility, Compute | Storage);
                passData.ClustersVisibility = graph.Write(clusterVisibility, Compute | Storage);
                passData.ActiveClusters = graph.Write(passData.ActiveClusters, Compute | Storage);
                passData.ActiveClustersCount = graph.Read(passData.ActiveClustersCount, Compute | Storage | Upload);
                passData.ActiveClustersCount = graph.Write(passData.ActiveClustersCount, Compute | Storage);

                graph.UpdateBlackboard(passData);
            },
            [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
            {
                CPU_PROFILE_FRAME("Lights.Clusters.Compact")
                GPU_PROFILE_FRAME("Lights.Clusters.Compact")

                const Shader& shader = resources.GetGraph()->GetShader();
                auto& pipeline = shader.Pipeline(); 
                auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

                resourceDescriptors.UpdateBinding("u_clusters", resources.GetBuffer(passData.Clusters).BindingInfo());
                resourceDescriptors.UpdateBinding("u_cluster_visibility", resources.GetBuffer(
                    passData.ClustersVisibility).BindingInfo());
                resourceDescriptors.UpdateBinding("u_active_clusters", resources.GetBuffer(
                    passData.ActiveClusters).BindingInfo());
                resourceDescriptors.UpdateBinding("u_count", resources.GetBuffer(
                    passData.ActiveClustersCount, 0, *frameContext.ResourceUploader).BindingInfo());

                auto& cmd = frameContext.Cmd;
                pipeline.BindCompute(cmd);
                resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

                RenderCommand::Dispatch(cmd,
                    {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y * LIGHT_CLUSTER_BINS_Z, 1},
                    {8, 8, 1});
            });
    }
}

RG::Pass& Passes::LightClustersCompact::addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource clusters,
    RG::Resource& depth)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            auto& identify = identifyActiveClusters(std::format("{}.Identify", name), graph, clusters, depth);
            auto& identifyOutput = graph.GetBlackboard().Get<PassData>(identify);
            auto& compact = compactActiveClusters(std::format("{}.Compact", name), graph, clusters,
                identifyOutput.ClustersVisibility);
            auto& compactOutput = graph.GetBlackboard().Get<PassData>(compact);
            compactOutput.Depth = depth;
            
            graph.UpdateBlackboard(compactOutput);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
        });
}
