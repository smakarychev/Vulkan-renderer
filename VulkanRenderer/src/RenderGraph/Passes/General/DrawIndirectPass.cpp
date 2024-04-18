#include "DrawIndirectPass.h"

#include "CameraGPU.h"
#include "FrameContext.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGUtils.h"
#include "RenderGraph/RGGeometry.h"
#include "Vulkan/RenderCommand.h"

DrawIndirectPass::DrawIndirectPass(RG::Graph& renderGraph, std::string_view name,
    const DrawIndirectPassInitInfo& info)
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
        
        m_PipelineData.MaterialDescriptors = *info.MaterialDescriptors;
    }
}

void DrawIndirectPass::AddToGraph(RG::Graph& renderGraph, const DrawIndirectPassExecutionInfo& info)
{
    using namespace RG;
    using enum ResourceAccessFlags;

    m_Pass = &renderGraph.AddRenderPass<PassDataPrivate>(m_Name,
        [&](Graph& graph, PassDataPrivate& passData)
        {
            passData.CameraUbo = graph.CreateResource(
                m_Name.Name() + ".Camera", GraphBufferDescription{.SizeBytes = sizeof(CameraGPU)});
            passData.CameraUbo = graph.Read(passData.CameraUbo, Vertex | Pixel | Uniform | Upload);
            
            passData.AttributeBuffers = RgUtils::readDrawAttributes(*info.Geometry, graph, m_Name.Name(), Vertex);
            
            passData.ObjectsSsbo = graph.AddExternal(m_Name.Name() + ".Objects",
                info.Geometry->GetRenderObjectsBuffer());
            passData.CommandsIndirect = graph.Read(info.Commands, Vertex | Indirect);

            passData.DrawAttachmentResources = RgUtils::readWriteDrawAttachments(info.DrawAttachments, graph);
            
            if (enumHasAny(m_Features, DrawFeatures::IBL))
            {
                ASSERT(info.IBL.has_value(), "IBL data is not provided")
                passData.IBL = {
                    .Irradiance = graph.Read(info.IBL->Irradiance, Pixel | Sampled),
                    .PrefilterEnvironment = graph.Read(info.IBL->PrefilterEnvironment, Pixel | Sampled),
                    .BRDF = graph.Read(info.IBL->BRDF, Pixel | Sampled)};
            }
            if (enumHasAny(m_Features, DrawFeatures::SSAO))
            {
                ASSERT(info.SSAO.has_value(), "SSAO data is not provided")
                passData.SSAO->SSAOTexture = graph.Read(info.SSAO->SSAOTexture, Pixel | Sampled);
            }
            
            passData.PipelineData = &m_PipelineData;
            passData.DrawFeatures = m_Features;

            PassData passDataPublic = {};
            passDataPublic.DrawAttachmentResources  = passData.DrawAttachmentResources;
            graph.GetBlackboard().Update(m_Name.Hash(), passDataPublic);
        },
        [=](PassDataPrivate& passData, FrameContext& frameContext, const Resources& resources)
        {
            GPU_PROFILE_FRAME("Draw indirect")

            using enum DrawFeatures;

            CameraGPU cameraGPU = CameraGPU::FromCamera(*info.Camera, info.Resolution);
            const Buffer& cameraUbo = resources.GetBuffer(passData.CameraUbo, cameraGPU,
                *frameContext.ResourceUploader);
            const Buffer& objectsSsbo = resources.GetBuffer(passData.ObjectsSsbo);
            const Buffer& commandsDraw = resources.GetBuffer(passData.CommandsIndirect);

            auto& pipeline = passData.PipelineData->Pipeline;
            auto& resourceDescriptors = passData.PipelineData->ResourceDescriptors;

            resourceDescriptors.UpdateBinding("u_camera", cameraUbo.BindingInfo());

            RgUtils::updateDrawAttributeBindings(resourceDescriptors, resources,
                passData.AttributeBuffers, passData.DrawFeatures);
            
            resourceDescriptors.UpdateBinding("u_objects", objectsSsbo.BindingInfo());
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
            RenderCommand::DrawIndexedIndirect(cmd,
                commandsDraw, offsetCommands * sizeof(IndirectDrawCommand),
                toDrawCommands);
        });
}
