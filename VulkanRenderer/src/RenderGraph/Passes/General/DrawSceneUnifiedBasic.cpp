#include "DrawSceneUnifiedBasic.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneUgbBindGroup.generated.h"
#include "Scene/Scene.h"

RG::Pass& Passes::DrawSceneUnifiedBasic::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Camera{};
        Resource UGB{};
        Resource Objects{};
        Resource Commands{};
        DrawAttachmentResources Attachments{};
    };

    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Draw.SceneUnifiedBasic.Setup")

            graph.SetShader("scene-ugb.shader");

            passData.Camera = graph.CreateResource(
                std::format("{}.Camera", name), GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.Camera = graph.Read(passData.Camera, Vertex | Pixel | Uniform);
            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            graph.Upload(passData.Camera, cameraGPU);

            passData.UGB = graph.AddExternal(std::format("{}.UGB", name),
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.Read(passData.UGB, Vertex | Pixel | Storage);
            
            passData.Objects = graph.AddExternal(std::format("{}.Objects", name),
                info.Geometry->RenderObjects);
            passData.Objects = graph.Read(passData.Objects, Vertex | Pixel | Storage);
            
            passData.Commands = graph.AddExternal(std::format("{}.Commands", name), info.Geometry->Commands);
            passData.Commands = graph.Read(passData.Commands, Vertex | Indirect);
            
            passData.Attachments = RgUtils::readWriteDrawAttachments(info.Attachments, graph);
            
            PassData passDataPublic = {};
            passDataPublic.Attachments = passData.Attachments;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Draw.SceneUnifiedBasic")
            GPU_PROFILE_FRAME("Draw.SceneUnifiedBasic")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneUgbShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            bindGroup.SetUGB({.Buffer = resources.GetBuffer(passData.UGB)});
            bindGroup.SetCommands({.Buffer = resources.GetBuffer(passData.Commands)});
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Objects)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirect({
                .Buffer = resources.GetBuffer(passData.Commands),
                .Count = info.Geometry->CommandCount});
        });

    return pass;
}
