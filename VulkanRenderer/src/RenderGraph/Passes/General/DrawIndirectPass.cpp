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
        Resource CommandsIndirect{};

        DrawAttachmentResources DrawAttachmentResources{};

        std::optional<IBLData> IBL{};
        std::optional<SSAOData> SSAO{};
    };

    Pass& pass = renderGraph.AddRenderPass<PassDataPrivate>(name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            CPU_PROFILE_FRAME("Draw.Indirect.Setup")

            graph.CopyShader(info.Shader);
            
            passData.Camera = graph.CreateResource(
                std::string{name} + ".Camera", GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.Camera = graph.Read(passData.Camera, Vertex | Pixel | Uniform);
            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            graph.Upload(passData.Camera, cameraGPU);
            
            passData.AttributeBuffers = RgUtils::readDrawAttributes(*info.Geometry, graph, std::string{name}, Vertex);
            
            passData.Objects = graph.AddExternal(std::string{name} + ".Objects",
                info.Geometry->GetRenderObjectsBuffer());
            passData.CommandsIndirect = graph.Read(info.Commands, Vertex | Indirect);

            passData.DrawAttachmentResources = RgUtils::readWriteDrawAttachments(info.DrawInfo.Attachments, graph);
            
            if (enumHasAny(info.Shader->Features(), DrawFeatures::IBL))
            {
                ASSERT(info.DrawInfo.IBL.has_value(), "IBL data is not provided")
                passData.IBL = RgUtils::readIBLData(*info.DrawInfo.IBL, graph, Pixel);
            }
            if (enumHasAny(info.Shader->Features(), DrawFeatures::SSAO))
            {
                ASSERT(info.DrawInfo.SSAO.has_value(), "SSAO data is not provided")
                passData.SSAO = RgUtils::readSSAOData(*info.DrawInfo.SSAO, graph, Pixel);
            }
            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources  = passData.DrawAttachmentResources;
            
            graph.UpdateBlackboard(passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            CPU_PROFILE_FRAME("Draw.Indirect")
            GPU_PROFILE_FRAME("Draw.Indirect")

            using enum DrawFeatures;

            const Buffer& cameraBuffer = resources.GetBuffer(passData.Camera);
            const Buffer& objects = resources.GetBuffer(passData.Objects);
            const Buffer& commandsDraw = resources.GetBuffer(passData.CommandsIndirect);

            auto& shader = resources.GetGraph()->GetShader();
            auto pipeline = shader.Pipeline();
            auto& resourceDescriptors = shader.Descriptors(ShaderDescriptorsKind::Resource);

            resourceDescriptors.UpdateBinding("u_camera", cameraBuffer.BindingInfo());

            RgUtils::updateDrawAttributeBindings(resourceDescriptors, resources,
                passData.AttributeBuffers, shader.Features());
            
            resourceDescriptors.UpdateBinding("u_objects", objects.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commandsDraw.BindingInfo());

            if (enumHasAny(shader.Features(), IBL))
                RgUtils::updateIBLBindings(resourceDescriptors, resources, *passData.IBL);
                
            if (enumHasAny(shader.Features(), SSAO))
                RgUtils::updateSSAOBindings(resourceDescriptors, resources, *passData.SSAO);

            auto& cmd = frameContext.Cmd;
            if (enumHasAny(shader.Features(), Textures))
            {
                RenderCommand::BindGraphics(cmd, pipeline);
                shader.Descriptors(ShaderDescriptorsKind::Sampler).BindGraphicsImmutableSamplers(
                    cmd, shader.GetLayout());
                shader.Descriptors(ShaderDescriptorsKind::Materials).BindGraphics(cmd,
                    resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            }

            RenderCommand::BindIndexU8Buffer(cmd, info.Geometry->GetAttributeBuffers().Indices, 0);
            
            RenderCommand::BindGraphics(cmd, pipeline);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), shader.GetLayout());
            u32 offsetCommands = std::min(info.CommandsOffset, info.Geometry->GetMeshletCount());
            u32 toDrawCommands = info.Geometry->GetMeshletCount() - offsetCommands;
            RenderCommand::PushConstants(cmd, shader.GetLayout(), offsetCommands);
            RenderCommand::DrawIndexedIndirect(cmd,
                commandsDraw, offsetCommands * sizeof(IndirectDrawCommand),
                toDrawCommands);
        });

    return pass;
}
