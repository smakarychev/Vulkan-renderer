#include "DrawIndirectPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Draw::Indirect::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const DrawIndirectPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Camera{};
        DrawAttributeBuffers AttributeBuffers{};
        Resource Objects{};
        Resource Commands{};

        DrawAttachmentResources DrawAttachmentResources{};
    };

    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Draw.Indirect.Setup")

            passData.Camera = graph.CreateResource(
                std::string{name} + ".Camera", GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.Camera = graph.Read(passData.Camera, Vertex | Pixel | Uniform);
            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            graph.Upload(passData.Camera, cameraGPU);
            
            passData.AttributeBuffers = RgUtils::readDrawAttributes(*info.Geometry, graph, std::string{name}, Vertex);
            
            passData.Objects = graph.AddExternal(std::string{name} + ".Objects",
                info.Geometry->GetRenderObjectsBuffer());
            passData.Commands = graph.Read(info.Commands, Vertex | Indirect);

            passData.DrawAttachmentResources = RgUtils::readWriteDrawAttachments(info.DrawInfo.Attachments, graph);
            
            info.DrawInfo.DrawSetup(graph);
            
            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources  = passData.DrawAttachmentResources;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Draw.Indirect")
            GPU_PROFILE_FRAME("Draw.Indirect")

            Buffer commandsDraw = resources.GetBuffer(passData.Commands);

            auto& cmd = frameContext.Cmd;
            
            const Shader& shader = info.DrawInfo.DrawBind(cmd, resources, {
                .Camera = passData.Camera,
                .Objects = passData.Objects,
                .Commands = passData.Commands,
                .DrawAttributes = passData.AttributeBuffers});

            RenderCommand::BindIndexU8Buffer(cmd, info.Geometry->GetAttributeBuffers().Indices, 0);
            RenderCommand::BindGraphics(cmd, shader.Pipeline());
            u32 offsetCommands = std::min(info.CommandsOffset, info.Geometry->GetMeshletCount());
            u32 toDrawCommands = info.Geometry->GetMeshletCount() - offsetCommands;
            RenderCommand::PushConstants(cmd, shader.GetLayout(), offsetCommands);
            RenderCommand::DrawIndexedIndirect(cmd,
                commandsDraw, offsetCommands * sizeof(IndirectDrawCommand),
                toDrawCommands);
        });

    return pass;
}
