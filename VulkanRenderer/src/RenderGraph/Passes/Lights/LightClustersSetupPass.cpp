#include "LightClustersSetupPass.h"

#include "Light/Light.h"
#include "RenderGraph/RGGraph.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Core/Camera.h"
#include "cvars/CVarSystem.h"
#include "RenderGraph/Passes/Generated/LightClustersSetupBindGroup.generated.h"

Passes::LightClustersSetup::PassData& Passes::LightClustersSetup::addToGraph(StringId name, RG::Graph& renderGraph)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Setup.Setup")

            graph.SetShader("light-clusters-setup"_hsv);

            passData.Clusters = graph.Create("Clusters"_hsv, RGBufferDescription{
                .SizeBytes = LIGHT_CLUSTER_BINS * sizeof(LightCluster)});
            passData.ClusterVisibility = graph.Create("Cluster.Visibility"_hsv,
                RGBufferDescription{.SizeBytes = LIGHT_CLUSTER_BINS * sizeof(u8)});
            passData.Clusters = graph.WriteBuffer(passData.Clusters, Compute | Storage);
        },
        [=](const PassData& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Setup")
            GPU_PROFILE_FRAME("Lights.Clusters.Setup")

            const Shader& shader = graph.GetShader();
            LightClustersSetupShaderBindGroup bindGroup(shader);

            bindGroup.SetClusters(graph.GetBufferBinding(passData.Clusters));
            bindGroup.SetClusterVisibility(graph.GetBufferBinding(passData.ClusterVisibility));

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
                .Far = *CVars::Get().GetF32CVar("Renderer.Limits.MaxLightCullDistance"_hsv),
                .ProjectionInverse = glm::inverse(frameContext.PrimaryCamera->GetProjection())};

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {pushConstant}});
            cmd.Dispatch({
                .Invocations = {LIGHT_CLUSTER_BINS_X, LIGHT_CLUSTER_BINS_Y, LIGHT_CLUSTER_BINS_Z},
                .GroupSize = {1, 1, 1}});
        });
}
