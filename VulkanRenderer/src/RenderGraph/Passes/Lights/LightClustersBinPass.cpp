#include "LightClustersBinPass.h"

#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/ShaderCache.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::LightClustersBin::addToGraph(std::string_view name, RG::Graph& renderGraph,
    RG::Resource dispatchIndirect, RG::Resource clusters, RG::Resource activeClusters, RG::Resource clustersCount,
    const SceneLight& sceneLight)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin.Setup")

            graph.SetShader("../assets/shaders/light-clusters-bin.shader");

            passData.Dispatch = graph.Read(dispatchIndirect, Indirect);

            passData.Clusters = graph.Read(clusters, Compute | Storage);
            passData.Clusters = graph.Write(passData.Clusters, Compute | Storage);
            
            passData.ActiveClusters = graph.Read(activeClusters, Compute | Storage);

            passData.ClusterCount = graph.Read(clustersCount, Compute | Storage);

            passData.SceneLightResources = RgUtils::readSceneLight(sceneLight, graph, Compute);
            
            graph.UpdateBlackboard(passData);
        },
        [=](PassData& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin")
            GPU_PROFILE_FRAME("Lights.Clusters.Bin")

            const Shader& shader = resources.GetGraph()->GetShader();
            auto& pipeline = shader.Pipeline(); 
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_clusters", resources.GetBuffer(passData.Clusters).BindingInfo());
            resourceDescriptors.UpdateBinding("u_active_clusters", resources.GetBuffer(
                passData.ActiveClusters).BindingInfo());
            resourceDescriptors.UpdateBinding("u_count", resources.GetBuffer(passData.ClusterCount).BindingInfo());
            resourceDescriptors.UpdateBinding("u_point_lights",
                resources.GetBuffer(passData.SceneLightResources.PointLights).BindingInfo());
            resourceDescriptors.UpdateBinding("u_lights_info",
                resources.GetBuffer(passData.SceneLightResources.LightsInfo).BindingInfo());

            auto& cmd = frameContext.Cmd;
            pipeline.BindCompute(cmd);
            RenderCommand::PushConstants(cmd, shader.GetLayout(), frameContext.PrimaryCamera->GetView());
            resourceDescriptors.BindCompute(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            RenderCommand::DispatchIndirect(cmd, resources.GetBuffer(passData.Dispatch), 0);
        });
}
