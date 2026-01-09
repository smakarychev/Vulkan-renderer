#include "rendererpch.h"

#include "ScenePrepareVisibleMeshletInfoPass.h"

#include "RenderGraph/Passes/Generated/ScenePrepareVisibileMeshletsBindGroupRG.generated.h"
#include "Scene/SceneRenderObjectSet.h"

Passes::ScenePrepareVisibleMeshletInfo::PassData& Passes::ScenePrepareVisibleMeshletInfo::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, ScenePrepareVisibileMeshletsBindGroupRG>;

    const u32 meshletCount = info.RenderObjectSet->MeshletCount();

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo.Setup")

            passData.BindGroup = ScenePrepareVisibileMeshletsBindGroupRG(graph);

            passData.MeshletInfoCount = passData.BindGroup.SetResourcesMeshletInfoCounts(
                graph.Create("MeshletInfoCount"_hsv, RGBufferDescription{.SizeBytes = sizeof(u32)}));
            passData.MeshletInfoCount = graph.Upload(passData.MeshletInfoCount, 0);
            
            passData.MeshletInfos = passData.BindGroup.SetResourcesMeshletInfos(
                graph.Create("MeshletInfos"_hsv,
                    RGBufferDescription{.SizeBytes = sizeof(SceneMeshletBucketInfo) * meshletCount}));

            passData.BindGroup.SetResourcesCommands(graph.Import("ReferenceCommands"_hsv,
                info.RenderObjectSet->Geometry().Commands.Buffer));
            passData.BindGroup.SetResourcesObjectBuckets(graph.Import("Buckets"_hsv,
                info.RenderObjectSet->BucketBits()));
            passData.BindGroup.SetResourcesMeshletsHandles(graph.Import("MeshletHandles"_hsv,
                info.RenderObjectSet->MeshletHandles()));
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo")
            GPU_PROFILE_FRAME("Scene.PrepareVisibleMeshletInfo")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindCompute(cmd, graph.GetFrameAllocators());
            cmd.PushConstants({
                .PipelineLayout = passData.BindGroup.Shader->GetLayout(),
                .Data = {meshletCount}
            });
            cmd.Dispatch({
                .Invocations = {meshletCount, 1, 1},
                .GroupSize = passData.BindGroup.GetPrepareVisibileMeshletsGroupSize()
            });
        });
}
