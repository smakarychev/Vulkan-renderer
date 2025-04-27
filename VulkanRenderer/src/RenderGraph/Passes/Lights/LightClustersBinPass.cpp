#include "LightClustersBinPass.h"

#include "Core/Camera.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/LightClustersBinBindGroup.generated.h"
#include "Rendering/Shader/ShaderCache.h"

RG::Pass& Passes::LightClustersBin::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Dispatch{};
        Resource Clusters{};
        Resource ActiveClusters{};
        Resource ClusterCount{};
        SceneLightResources SceneLightResources{};
    };
    
    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Lights.Clusters.Bin.Setup")

            graph.SetShader("light-clusters-bin.shader");

            passData.Dispatch = graph.Read(info.DispatchIndirect, Indirect);

            passData.Clusters = graph.Read(info.Clusters, Compute | Storage);
            passData.Clusters = graph.Write(passData.Clusters, Compute | Storage);
            
            passData.ActiveClusters = graph.Read(info.ActiveClusters, Compute | Storage);

            passData.ClusterCount = graph.Read(info.ClustersCount, Compute | Storage);

            passData.SceneLightResources = RgUtils::readSceneLight(*info.Light, graph, Compute);

            PassData passDataPublic = {};
            passDataPublic.Clusters = passData.Clusters;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
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
