#include "DrawSceneUnifiedBasic.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/Passes/Generated/SceneUgbBindGroup.generated.h"
#include "Scene/Scene.h"

RG::Pass& Passes::DrawSceneUnifiedBasic::addToGraph(StringId name, RG::Graph& renderGraph,
    const ExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Camera{};
        Resource UGB{};
        Resource Objects{};
        Resource Draws{};
        Resource DrawInfos{};
        DrawAttachmentResources Attachments{};
        SceneLightResources Light{};
    };

    return renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Scene.DrawUnifiedBasic.Setup")

            graph.SetShader("scene-ugb.shader");

            passData.Camera = graph.CreateResource("Camera"_hsv,
                GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.Camera = graph.Read(passData.Camera, Vertex | Pixel | Uniform);
            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            graph.Upload(passData.Camera, cameraGPU);

            passData.UGB = graph.AddExternal("UGB"_hsv,
                Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Attributes));
            passData.UGB = graph.Read(passData.UGB, Vertex | Pixel | Storage);
            
            passData.Objects = graph.AddExternal("Objects"_hsv,
                info.Geometry->RenderObjects.Buffer);
            passData.Objects = graph.Read(passData.Objects, Vertex | Pixel | Storage);
            
            passData.Draws = graph.Read(info.Draws, Vertex | Indirect);
            passData.DrawInfos = graph.Read(info.DrawInfos, Vertex | Indirect);
            
            passData.Attachments = RgUtils::readWriteDrawAttachments(info.Attachments, graph);
            passData.Light = RgUtils::readSceneLight(*info.Lights, graph, Pixel);
            
            PassData passDataPublic = {};
            passDataPublic.Attachments = passData.Attachments;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Scene.DrawUnifiedBasic")
            GPU_PROFILE_FRAME("Scene.DrawUnifiedBasic")

            const Shader& shader = resources.GetGraph()->GetShader();
            SceneUgbShaderBindGroup bindGroup(shader);
            bindGroup.SetCamera({.Buffer = resources.GetBuffer(passData.Camera)});
            bindGroup.SetUGB({.Buffer = resources.GetBuffer(passData.UGB)});
            bindGroup.SetCommands({.Buffer = resources.GetBuffer(passData.Draws)});
            bindGroup.SetObjects({.Buffer = resources.GetBuffer(passData.Objects)});
            bindGroup.SetDirectionalLights({.Buffer = resources.GetBuffer(passData.Light.DirectionalLights)});
            bindGroup.SetPointLights({.Buffer = resources.GetBuffer(passData.Light.PointLights)});
            bindGroup.SetLightsInfo({.Buffer = resources.GetBuffer(passData.Light.LightsInfo)});

            auto& cmd = frameContext.CommandList;
            bindGroup.Bind(cmd, resources.GetGraph()->GetArenaAllocators());
            cmd.BindIndexU8Buffer({
                .Buffer = Device::GetBufferArenaUnderlyingBuffer(info.Geometry->Indices)});
            cmd.DrawIndexedIndirectCount({
                .DrawBuffer = resources.GetBuffer(passData.Draws),
                .CountBuffer = resources.GetBuffer(passData.DrawInfos),
                .MaxCount = info.Geometry->CommandCount});
        });
}
