#include "SceneVBufferPass.h"

#include "RenderGraph/Passes/Generated/SceneVbufferUgbBindGroup.generated.h"

RG::Pass& Passes::SceneVBuffer::addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        SceneDrawPassResources Resources{};
        Resource UGB{};
        Resource Objects{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.VBufferUgb.Setup")

            graph.SetShader("scene-vbuffer-ugb"_hsv, *info.DrawInfo.BucketOverrides);

            passData.Resources.CreateFrom(info.DrawInfo, graph);

            passData.UGB = graph.AddExternal("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.Read(passData.UGB, Vertex | Pixel | Storage);
            
            passData.Objects = graph.AddExternal("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.Read(passData.Objects, Vertex | Pixel | Storage);

            PassData passDataPublic = {};
            passDataPublic.Attachments = passData.Resources.Attachments;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Scene.VBufferUgb")
            GPU_PROFILE_FRAME("Scene.VBufferUgb")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneVbufferUgbShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Resources.Camera)});
            bindGroup.SetUGB({.Buffer = resources.GetBuffer(passData.UGB)});
            bindGroup.SetCommands({.Buffer = resources.GetBuffer(passData.Resources.Draws)});
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Objects)});
            
            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetFrameAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = resources.GetBuffer(passData.Resources.Draws),
                .CountBuffer = resources.GetBuffer(passData.Resources.DrawInfo),
                .MaxCount = passData.Resources.MaxDrawCount});
        });
}
