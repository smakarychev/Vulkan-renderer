#include "BRDFProcessor.h"

#include "ProcessingResources.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Synchronization.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

Texture BRDFProcessor::CreateBRDF(const CommandBuffer& cmd)
{
    static DescriptorArenaAllocators allocators = ProcessingResources::Allocators();
    static ProcessingPipeline pipelineData = ProcessingResources::CreatePipeline(
    ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/brdf-comp.stage"},
         "BRDFProcessor", allocators));

    auto&& [pipeline, samplerDescriptors, resourceDescriptors] = pipelineData;

    static Texture brdf = Texture::Builder({
            .Width = BRDF_RESOLUTION,
            .Height = BRDF_RESOLUTION,
            .Format = Format::RG16_FLOAT,
            .Usage = ImageUsage::Sampled | ImageUsage::Storage})
        .Build();

    struct PushConstants
    {
        glm::vec2 BRDFResolutionInverse{};
    };
    PushConstants pushConstants = {
        .BRDFResolutionInverse = 1.0f / glm::vec2((f32)BRDF_RESOLUTION)};
    
    DeletionQueue deletionQueue = {};
    
    resourceDescriptors.UpdateBinding("u_brdf", brdf.BindingInfo(ImageFilter::Linear, ImageLayout::General));

    RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder().LayoutTransition({
            .ImageSubresource = brdf.Subresource(),
            .SourceStage = PipelineStage::Top,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::WriteShader,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::General})
        .SetFlags(PipelineDependencyFlags::ByRegion)
        .Build(deletionQueue));

    RenderCommand::Bind(cmd, allocators);
    pipeline.BindCompute(cmd);
    RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
    resourceDescriptors.BindCompute(cmd, allocators, pipeline.GetLayout());
    RenderCommand::Dispatch(cmd,
        {BRDF_RESOLUTION, BRDF_RESOLUTION, 1},
        {32, 32, 1});

    RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder().LayoutTransition({
            .ImageSubresource = brdf.Subresource(),
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::PixelShader | PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadSampled,
            .OldLayout = ImageLayout::General,
            .NewLayout = ImageLayout::Readonly})
        .SetFlags(PipelineDependencyFlags::ByRegion)
        .Build(deletionQueue));

    Fence processFence = Fence::Builder().Build(deletionQueue);
    cmd.End();
    cmd.Submit(Driver::GetDevice().GetQueues().Graphics, processFence);
    processFence.Wait();
    deletionQueue.Flush();
    cmd.Begin();

    return brdf;
}
