#include "LightClustersBinPass.h"

#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/LightClustersBinBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::LightClustersBin::addToGraph(StringId name, RG::Graph& renderGraph,
    RG::Resource dispatchIndirect, RG::Resource clusters, RG::Resource activeClusters, RG::Resource clustersCount,
    const SceneLight& sceneLight)
{
    using namespace RG;
    using enum ResourceAccessFlags;
    
    return renderGraph.AddRenderPass<PassData>(name,
        [&](Graph& graph, PassData& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin.Setup")

            graph.SetShader("light-clusters-bin.shader");

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
            LightClustersBinShaderBindGroup bindGroup(shader);
            bindGroup.SetClusters({.Buffer = resources.GetBuffer(passData.Clusters)});
            bindGroup.SetActiveClusters({.Buffer = resources.GetBuffer(passData.ActiveClusters)});
            bindGroup.SetCount({.Buffer = resources.GetBuffer(passData.ClusterCount)});
            bindGroup.SetPointLights({.Buffer = resources.GetBuffer(passData.SceneLightResources.PointLights)});
            bindGroup.SetLightsInfo({.Buffer = resources.GetBuffer(passData.SceneLightResources.LightsInfo)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(frameContext.CommandList, resources.GetGraph()->GetArenaAllocators());
            cmd.PushConstants({
                .PipelineLayout = shader.GetLayout(), 
                .Data = {frameContext.PrimaryCamera->GetView()}});
            cmd.DispatchIndirect({
                .Buffer = resources.GetBuffer(passData.Dispatch)});
        });
}
