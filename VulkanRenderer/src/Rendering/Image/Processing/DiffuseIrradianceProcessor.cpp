#include "DiffuseIrradianceProcessor.h"

#include "ProcessingResources.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Shader.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

std::vector<std::pair<Image, Image>> DiffuseIrradianceProcessor::s_PendingTextures = {};

void DiffuseIrradianceProcessor::Add(const Image& source, const Image& irradiance)
{
    s_PendingTextures.emplace_back(std::make_pair(source, irradiance));
}

void DiffuseIrradianceProcessor::Process(const CommandBuffer& cmd)
{
    static DescriptorArenaAllocators allocators = ProcessingResources::Allocators();
    static ProcessingPipeline pipelineData = ProcessingResources::CreatePipeline(
    ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/diffuse-irradiance-comp.stage"},
         "DiffuseIrradianceProcessor", allocators));

    auto&& [pipeline, samplerDescriptors, resourceDescriptors] = pipelineData;

    DeletionQueue deletionQueue = {};
    RenderCommand::Bind(cmd, allocators);
    samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
    pipeline.BindCompute(cmd);
    resourceDescriptors.BindCompute(cmd, allocators, pipeline.GetLayout());

    for (auto&& [source, irradiance] : s_PendingTextures)
    {
        LayoutTransitionInfo toGeneral = {
            .ImageSubresource = irradiance.Subresource(),
            .SourceStage = PipelineStage::Top,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::WriteShader,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::General};

        LayoutTransitionInfo toReadOnly = {
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage =  PipelineStage::PixelShader | PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadSampled,
            .OldLayout = ImageLayout::General,
            .NewLayout = ImageLayout::Readonly};

        resourceDescriptors.UpdateBinding("u_env", source.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
        resourceDescriptors.UpdateBinding("u_irradiance", irradiance.BindingInfo(
            ImageFilter::Linear, ImageLayout::General));
        
        struct PushConstants
        {
            glm::vec2 IrradianceResolutionInverse{};
        };
        PushConstants pushConstants = {
            .IrradianceResolutionInverse = 1.0f / glm::vec2{(f32)irradiance.Description().Width}};

        RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
            .LayoutTransition(toGeneral)
            .SetFlags(PipelineDependencyFlags::ByRegion)
            .Build(deletionQueue));
        RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
        RenderCommand::Dispatch(cmd,
            {irradiance.Description().Width, irradiance.Description().Width, 6},
            {32, 32, 1});
        
        toReadOnly.ImageSubresource = irradiance.Subresource();
        RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
            .LayoutTransition(toReadOnly)
            .SetFlags(PipelineDependencyFlags::ByRegion)
            .Build(deletionQueue));
    }
    
    Fence processFence = Fence::Builder().Build(deletionQueue);
    cmd.End();
    cmd.Submit(Driver::GetDevice().GetQueues().Graphics, processFence);
    processFence.Wait();
    deletionQueue.Flush();
    cmd.Begin();

    s_PendingTextures.clear();
}

Texture DiffuseIrradianceProcessor::CreateEmptyTexture()
{
    return Texture::Builder({
            .Width = IRRADIANCE_RESOLUTION,
            .Height = IRRADIANCE_RESOLUTION,
            .Layers = 6,
            .Format = Format::RGBA16_FLOAT,
            .Kind = ImageKind::Cubemap,
            .Usage = ImageUsage::Sampled | ImageUsage::Storage})
        .Build();
}
