#include "DrawIndirectCountPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RGUtils.h"
#include "Scene/SceneGeometry.h"
#include "Vulkan/RenderCommand.h"

DrawIndirectCountPass::DrawIndirectCountPass(RG::Graph& renderGraph, std::string_view name, 
    const DrawIndirectCountPassInitInfo& info)
        : m_Name(name), m_Features(info.DrawFeatures)
{
    m_PipelineData.Pipeline = info.DrawPipeline;

    m_PipelineData.ResourceDescriptors = ShaderDescriptors::Builder()
        .SetTemplate(info.DrawPipeline.GetTemplate(), DescriptorAllocatorKind::Resources)
        .ExtractSet(1)
        .Build();

    if (enumHasAny(m_Features, RG::DrawFeatures::Textures))
    {
        ASSERT(info.MaterialDescriptors.has_value(), "Material desciptors are not provided")
        
        m_PipelineData.ImmutableSamplerDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(info.DrawPipeline.GetTemplate(), DescriptorAllocatorKind::Samplers)
            .ExtractSet(0)
            .Build();
        
        m_PipelineData.MaterialDescriptors = **info.MaterialDescriptors;
    }
}

void DrawIndirectCountPass::AddToGraph(RG::Graph& renderGraph, const DrawIndirectCountPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    m_Pass = &renderGraph.AddRenderPass<PassDataPrivate>(m_Name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            passData.Camera = graph.CreateResource(
                m_Name.Name() + ".Camera", GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.Camera = graph.Read(passData.Camera, Vertex | Pixel | Uniform | Upload);

            passData.AttributeBuffers = RgUtils::readDrawAttributes(*info.Geometry, graph, m_Name.Name(), Vertex);

            passData.Objects = graph.AddExternal(m_Name.Name() + ".Objects",
                info.Geometry->GetRenderObjectsBuffer());
            passData.Commands = graph.Read(info.Commands, Vertex | Indirect);
            passData.Count = graph.Read(info.CommandCount, Vertex | Indirect);

            passData.DrawAttachmentResources = RgUtils::readWriteDrawAttachments(info.DrawAttachments, graph);

            if (enumHasAny(m_Features, DrawFeatures::IBL))
            {
                ASSERT(info.IBL.has_value(), "IBL data is not provided")
                passData.IBL = RgUtils::readIBLData(*info.IBL, graph, Pixel);
            }
            if (enumHasAny(m_Features, DrawFeatures::SSAO))
            {
                ASSERT(info.SSAO.has_value(), "SSAO data is not provided")
                passData.SSAO = RgUtils::readSSAOData(*info.SSAO, graph, Pixel);
            }
            
            passData.PipelineData = &m_PipelineData;
            passData.DrawFeatures = m_Features;
            
            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources  = passData.DrawAttachmentResources;
            graph.GetBlackboard().Update(m_Name.Hash(), passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Draw indirect count")

            using enum DrawFeatures;

            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            const Buffer& camera = resources.GetBuffer(passData.Camera, cameraGPU,
               *frameContext.ResourceUploader);
            const Buffer& objects = resources.GetBuffer(passData.Objects);
            const Buffer& commandsDraw = resources.GetBuffer(passData.Commands);
            const Buffer& countDraw = resources.GetBuffer(passData.Count);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_camera", camera.BindingInfo());

            RgUtils::updateDrawAttributeBindings(resourceDescriptors, resources,
                passData.AttributeBuffers, passData.DrawFeatures);
            
            resourceDescriptors.UpdateBinding("u_objects", objects.BindingInfo());
            resourceDescriptors.UpdateBinding("u_commands", commandsDraw.BindingInfo());

            if (enumHasAny(passData.DrawFeatures, IBL))
                RgUtils::updateIBLBindings(resourceDescriptors, resources, *passData.IBL);
                
            if (enumHasAny(passData.DrawFeatures, SSAO))
                RgUtils::updateSSAOBindings(resourceDescriptors, resources, *passData.SSAO);

            if (enumHasAny(passData.DrawFeatures, Textures))
            {
                pipeline.BindGraphics(frameContext.Cmd);
                passData.PipelineData->ImmutableSamplerDescriptors.BindGraphicsImmutableSamplers(
                    frameContext.Cmd, pipeline.GetLayout());
                passData.PipelineData->MaterialDescriptors.BindGraphics(frameContext.Cmd,
                    resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());
            }

            auto& cmd = frameContext.Cmd;
            RenderCommand::BindIndexU8Buffer(cmd, info.Geometry->GetAttributeBuffers().Indices, 0);
            
            pipeline.BindGraphics(cmd);
            resourceDescriptors.BindGraphics(cmd, resources.GetGraph()->GetArenaAllocators(), pipeline.GetLayout());

            u32 offsetCommands = std::min(info.CommandsOffset, info.Geometry->GetMeshletCount());
            u32 toDrawCommands = info.Geometry->GetMeshletCount() - offsetCommands;
            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), offsetCommands);
            RenderCommand::DrawIndexedIndirectCount(cmd,
                commandsDraw, offsetCommands * sizeof(IndirectDrawCommand),
                countDraw, 0,
                toDrawCommands);
        });
}
