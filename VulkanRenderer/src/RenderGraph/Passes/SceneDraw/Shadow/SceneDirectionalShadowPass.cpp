#include "rendererpch.h"

#include "SceneDirectionalShadowPass.h"

#include "ViewInfoGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/ShadowBindGroup.generated.h"
#include "RenderGraph/Passes/Generated/ShadowDirectionalUgbBindGroupRG.generated.h"

Passes::SceneDirectionalShadow::PassData& Passes::SceneDirectionalShadow::addToGraph(StringId name,
    RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassDataWithBind<PassData, ShadowDirectionalUgbBindGroupRG>
    {
        Resource UGB{};
        Resource Objects{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("SceneDirectionalShadow.Setup")

            passData.BindGroup = ShadowDirectionalUgbBindGroupRG(graph,
                graph.SetShader("shadowDirectionalUgb"_hsv, *info.DrawInfo.BucketOverrides));

            passData.Resources.InitFrom(info.DrawInfo, graph);
            passData.UGB = passData.BindGroup.SetResourcesUgb(graph.Import("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes)));
            passData.Objects = passData.BindGroup.SetResourcesRenderObjects(graph.Import("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer));
            passData.Resources.ViewInfo = passData.BindGroup.SetResourcesView(info.DrawInfo.ViewInfo);
            passData.BindGroup.SetResourcesCommands(graph.Import("Commands"_hsv, info.Geometry->Commands.Buffer));
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("SceneDirectionalShadow")
            GPU_PROFILE_FRAME("SceneDirectionalShadow")

            auto& cmd = frameContext.CommandList;
            passData.BindGroup.BindGraphics(cmd, graph.GetFrameAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer =  graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount});
        });
}
