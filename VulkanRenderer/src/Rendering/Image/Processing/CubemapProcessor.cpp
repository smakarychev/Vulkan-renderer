#include "CubemapProcessor.h"

#include "ProcessingResources.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Shader.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

std::unordered_map<std::string, Image> CubemapProcessor::s_PendingTextures = {};

void CubemapProcessor::Add(const std::string& path, const Image& image)
{
    s_PendingTextures.emplace(path, image);
}

void CubemapProcessor::Process(const CommandBuffer& cmd)
{
    static DescriptorArenaAllocators allocators = ProcessingResources::Allocators();
    static ProcessingPipeline pipelineData = ProcessingResources::CreatePipeline(
    ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/cubemap-processor-comp.stage"},
        "CubemapProcessor", allocators));

    auto&& [pipeline, samplerDescriptors, resourceDescriptors] = pipelineData;

    DeletionQueue deletionQueue = {};
    RenderCommand::Bind(cmd, allocators);
    samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
    pipeline.BindCompute(cmd);
    resourceDescriptors.BindCompute(cmd, allocators, pipeline.GetLayout());
    for (auto&& [path, cubemap] : s_PendingTextures)
    {
        Texture equirectangular = Texture::Builder({.Usage = ImageUsage::Sampled})
            .FromAssetFile(path)
            .Build(deletionQueue);

        LayoutTransitionInfo toGeneral = {
            .ImageSubresource = cubemap.Subresource(),
            .SourceStage = PipelineStage::Top,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::WriteShader,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::General};

        LayoutTransitionInfo toReadOnly = {
            .ImageSubresource = cubemap.Subresource(),
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage =  PipelineStage::PixelShader | PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadSampled,
            .OldLayout = ImageLayout::General,
            .NewLayout = ImageLayout::Readonly};

        resourceDescriptors.UpdateBinding("u_equirectangular", equirectangular.BindingInfo(
            ImageFilter::Linear, ImageLayout::Readonly));
        resourceDescriptors.UpdateBinding("u_cubemap", cubemap.BindingInfo(
            ImageFilter::Linear, ImageLayout::General));
        
        struct PushConstants
        {
            glm::vec2 CubemapResolutionInverse{};
        };
        PushConstants pushConstants = {
            .CubemapResolutionInverse = 1.0f / glm::vec2{(f32)cubemap.Description().Width}};
        
        RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
            .LayoutTransition(toGeneral)
            .SetFlags(PipelineDependencyFlags::ByRegion)
            .Build(deletionQueue));
        RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
        RenderCommand::Dispatch(cmd,
            {cubemap.Description().Width, cubemap.Description().Width, 6},
            {32, 32, 1});
        RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
            .LayoutTransition(toReadOnly)
            .SetFlags(PipelineDependencyFlags::ByRegion)
            .Build(deletionQueue));
    }
    
    Fence convertFence = Fence::Builder().Build(deletionQueue);
    cmd.End();
    cmd.Submit(Driver::GetDevice().GetQueues().Graphics, convertFence);
    convertFence.Wait();
    deletionQueue.Flush();
    cmd.Begin();

    // create mipmaps
    for (auto&& [path, cubemap] : s_PendingTextures)
        cubemap.CreateMipmaps(ImageLayout::Readonly);

    s_PendingTextures.clear();
}