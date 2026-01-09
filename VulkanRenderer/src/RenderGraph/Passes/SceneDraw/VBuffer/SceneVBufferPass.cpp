#include "rendererpch.h"

#include "SceneVBufferPass.h"

#include "RenderGraph/Passes/Generated/SceneDrawVbufferBindGroupRG.generated.h"

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
            passData.BindGroup.SetResourcesUgb(graph.Import("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes)));
            passData.BindGroup.SetResourcesRenderObjects(graph.Import("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer));
            passData.Resources.ViewInfo = passData.BindGroup.SetResourcesView(info.DrawInfo.ViewInfo);
            passData.BindGroup.SetResourcesCommands(graph.Import("Commands"_hsv, info.Geometry->Commands.Buffer));
        },
        [=](const PassDataBind& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.VBufferUgb")
            GPU_PROFILE_FRAME("Scene.VBufferUgb")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)
            });
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount
            });
        });
}
