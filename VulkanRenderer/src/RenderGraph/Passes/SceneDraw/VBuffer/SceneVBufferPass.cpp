#include "rendererpch.h"

#include "SceneVBufferPass.h"

#include "RenderGraph/Passes/Generated/SceneDrawVbufferBindGroupRG.generated.h"
#include "RenderGraph/Passes/Scene/SceneGeometryRGResources.h"

Passes::SceneVBuffer::PassData& Passes::SceneVBuffer::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using PassDataBind = PassDataWithBind<PassData, SceneDrawVbufferBindGroupRG>;

    return renderGraph.AddRenderPass<PassDataBind>(name,
        [&](Graph& graph, PassDataBind& passData)
        {
            CPU_PROFILE_FRAME("Scene.VBufferUgb.Setup")

            passData.BindGroup = SceneDrawVbufferBindGroupRG(graph, *info.DrawInfo.BucketOverrides);

            passData.Resources.InitFrom(info.DrawInfo, graph);
            passData.BindGroup.SetResourcesUgb(info.Geometry->Attributes);
            passData.BindGroup.SetResourcesRenderObjects(info.Geometry->RenderObjects);
            passData.BindGroup.SetResourcesMeshletsUgb(info.Geometry->Meshlets);
            passData.Resources.VisibleMeshlets =
                passData.BindGroup.SetResourcesVisibleMeshlets(passData.Resources.VisibleMeshlets);
            passData.BindGroup.SetResourcesMaterials(info.Geometry->Materials);
            passData.Resources.ViewInfo = passData.BindGroup.SetResourcesView(info.DrawInfo.ViewInfo);
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.VBufferUgb")
            GPU_PROFILE_FRAME("Scene.VBufferUgb")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd);
            cmd.BindIndexU8Buffer({
                .Buffer = graph.GetBuffer(info.Geometry->Indices)
            });
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount
            });
        });
}
