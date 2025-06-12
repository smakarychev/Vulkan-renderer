#include "SceneDepthPrepassPass.h"

#include "RenderGraph/RGGraph.h"
#include "RenderGraph/Passes/Generated/SceneDepthPrepassUgbBindGroup.generated.h"

Passes::SceneDepthPrepass::PassData& Passes::SceneDepthPrepass::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate : PassData
    {
        Resource UGB{};
        Resource Objects{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.SceneDepthPrepass.Setup")

            graph.SetShader("scene-depth-prepass-ugb"_hsv, *info.DrawInfo.BucketOverrides);

            passData.Resources.CreateFrom(info.DrawInfo, graph);

            passData.UGB = graph.Import("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.ReadBuffer(passData.UGB, Vertex | Pixel | Storage);
            
            passData.Objects = graph.Import("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.ReadBuffer(passData.Objects, Vertex | Pixel | Storage);
        },
        [=](const PassDataPrivate& passData, FrameContext& frameContext, const Graph& graph)
        {
            CPU_PROFILE_FRAME("Scene.SceneDepthPrepass")
            GPU_PROFILE_FRAME("Scene.SceneDepthPrepass")

            const Shader& shader = graph.GetShader();
            SceneDepthPrepassUgbShaderBindGroup bindGroup(shader);
            bindGroup.SetViewInfo(graph.GetBufferBinding(passData.Resources.ViewInfo));
            bindGroup.SetUGB(graph.GetBufferBinding(passData.UGB));
            bindGroup.SetCommands(graph.GetBufferBinding(passData.Resources.Draws));
            bindGroup.SetObjects(graph.GetBufferBinding(passData.Objects));

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, graph.GetFrameAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = graph.GetBuffer(passData.Resources.Draws),
                .CountBuffer = graph.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount});
        });
}
