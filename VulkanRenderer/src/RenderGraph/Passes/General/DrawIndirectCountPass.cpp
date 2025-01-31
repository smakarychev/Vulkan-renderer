#include "DrawIndirectCountPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Rendering/Shader/ShaderCache.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

RG::Pass& Passes::Draw::IndirectCount::addToGraph(std::string_view name, RG::Graph& renderGraph,
    const DrawIndirectCountPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    struct PassDataPrivate
    {
        Resource Camera{};
        DrawAttributeBuffers AttributeBuffers{};
        Resource Objects{};
        Resource Commands{};
        Resource Count{};
        
        DrawAttachmentResources DrawAttachmentResources{};
    };
    
    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Draw.Indirect.Count.Setup")

            passData.Camera = graph.CreateResource(
                std::string{name} + ".Camera", GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.Camera = graph.Read(passData.Camera, Vertex | Pixel | Uniform);
            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            graph.Upload(passData.Camera, cameraGPU);

            passData.AttributeBuffers = RgUtils::readDrawAttributes(*info.Geometry, graph, std::string{name}, Vertex);

            passData.Objects = graph.AddExternal(std::string{name} + ".Objects",
                info.Geometry->GetRenderObjectsBuffer());
            passData.Commands = graph.Read(info.Commands, Vertex | Indirect);
            passData.Count = graph.Read(info.CommandCount, Vertex | Indirect);

            passData.DrawAttachmentResources = RgUtils::readWriteDrawAttachments(info.DrawInfo.Attachments, graph);

            info.DrawInfo.DrawSetup(graph);

            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources  = passData.DrawAttachmentResources;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Draw.Indirect.Count")
            GPU_PROFILE_FRAME("Draw.Indirect.Count")

            Buffer commandsDraw = resources.GetBuffer(passData.Commands);
            Buffer countDraw = resources.GetBuffer(passData.Count);
            auto& cmd = frameContext.Cmd;
            
            const Shader& shader = info.DrawInfo.DrawBind(cmd, resources, {
                .Camera = passData.Camera,
                .Objects = passData.Objects,
                .Commands = passData.Commands,
                .DrawAttributes = passData.AttributeBuffers});

            RenderCommand::BindIndexU8Buffer(cmd, info.Geometry->GetAttributeBuffers().Indices, 0);
            u32 offsetCommands = std::min(info.CommandsOffset, info.Geometry->GetMeshletCount());
            u32 toDrawCommands = info.Geometry->GetMeshletCount() - offsetCommands;
            RenderCommand::PushConstants(cmd, shader.GetLayout(), offsetCommands);
            RenderCommand::DrawIndexedIndirectCount(cmd,
                commandsDraw, offsetCommands * sizeof(IndirectDrawCommand),
                countDraw, info.CountOffset * sizeof(u32),
                toDrawCommands);
        });

    return pass;
}
