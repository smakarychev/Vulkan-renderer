#include "LightClustersSetupPass.h"

#include "Light/Light.h"
#include "RenderGraph/RenderGraph.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"
#include "Core/Camera.h"

RG::Pass& Passes::LightClustersSetup::addToGraph(std::string_view name, RG::Graph& renderGraph)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Setup.Setup")

            graph.SetShader("../assets/shaders/light-clusters-setup.shader");

            passData.Clusters = graph.CreateResource(std::format("{}.Clusters", name), GraphBufferDescription{
                .SizeBytes = LIGHT_CLUSTER_BINS * sizeof(LightCluster)});
            passData.ClusterVisibility = graph.CreateResource(std::format("{}.Cluster.Visibility", name),
                GraphBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u8)});
            passData.Clusters = graph.Write(passData.Clusters, Compute | Storage);

            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Setup")
            GPU_PROFILE_FRAME("Lights.Clusters.Setup")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_clusters", resources.GetBuffer(passData.Clusters).BindingInfo());
            resourceDescriptors.UpdateBinding("u_cluster_visibility", resources.GetBuffer(
                passData.ClusterVisibility).BindingInfo());

            struct PushConstant
            {
                glm::vec2 RenderSize;
                f32 Near;
                f32 Far;
                glm::mat4 ProjectionInverse;
            };
            PushConstant pushConstant = {
                .RenderSize = frameContext.Resolution,
                .Near = frameContext.PrimaryCamera->GetNear(),
                .Far = frameContext.PrimaryCamera->GetFar(),
                .ProjectionInverse = glm::inverse(frameContext.PrimaryCamera->GetProjection())};

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstant);
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            RenderCommand::Dispatch(cmd,
                {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y, LIGHT_CLUSTER_BINS_Z},
                {1, 1, 1});
        });
}
