#include "EnvironmentPrefilterProcessor.h"

#include "ProcessingResources.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Synchronization.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

std::vector<std::pair<Image, Image>> EnvironmentPrefilterProcessor::s_PendingTextures = {};


void EnvironmentPrefilterProcessor::Add(const Image& source, const Image& prefiltered)
{
    s_PendingTextures.emplace_back(std::make_pair(source, prefiltered));
}

void EnvironmentPrefilterProcessor::Process(const CommandBuffer& cmd)
{
    static DescriptorArenaAllocators allocators = ProcessingResources::Allocators();
    static ShaderPipelineTemplate* pipelineTemplate = ShaderTemplateLibrary::LoadShaderPipelineTemplate({
        "../assets/shaders/processed/environment-prefilter-comp.shader"},
        "EnvironmentPrefilter", allocators);

    static const u32 MAX_MIPMAPS = Image::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION});
    
    ShaderPipeline pipeline{};
    ShaderDescriptors samplerDescriptors{};
    std::vector<ShaderDescriptors> resourceDescriptors(MAX_MIPMAPS);
    static std::once_flag once{};
    std::call_once(once, [&]()
    {
        pipeline = ShaderPipeline::Builder()
            .SetTemplate(pipelineTemplate)
            .UseDescriptorBuffer()
            .Build();

        samplerDescriptors = ShaderDescriptors::Builder()
            .SetTemplate(pipelineTemplate, DescriptorAllocatorKind::Samplers)
            .ExtractSet(0)
            .Build();

        for (u32 i = 0; i < MAX_MIPMAPS; i++)
            resourceDescriptors[i] = ShaderDescriptors::Builder()
                .SetTemplate(pipelineTemplate, DescriptorAllocatorKind::Resources)
                .ExtractSet(1)
                .Build();
    });
    
    DeletionQueue deletionQueue = {};
    RenderCommand::Bind(cmd, allocators);
    samplerDescriptors.BindComputeImmutableSamplers(cmd, pipeline.GetLayout());
    pipeline.BindCompute(cmd);
    
    for (auto&& [source, prefilter] : s_PendingTextures)
    {
        LayoutTransitionInfo toGeneral = {
            .ImageSubresource = prefilter.Subresource(),
            .SourceStage = PipelineStage::Top,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::None,
            .DestinationAccess = PipelineAccess::WriteShader,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::General};

        LayoutTransitionInfo toReadOnly = {
            .ImageSubresource = prefilter.Subresource(),
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage =  PipelineStage::PixelShader | PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadSampled,
            .OldLayout = ImageLayout::General,
            .NewLayout = ImageLayout::Readonly};
       
        u32 resolution = prefilter.Description().Width;
        auto viewHandles = prefilter.GetViewHandles();
        
        RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder()
            .LayoutTransition(toGeneral)
            .SetFlags(PipelineDependencyFlags::ByRegion)
            .Build(deletionQueue));
        for (u32 mipmap = 0; mipmap < prefilter.Description().Mipmaps; mipmap++)
        {
            resourceDescriptors[mipmap].UpdateBinding("u_env", source.BindingInfo(
                ImageFilter::Linear, ImageLayout::Readonly));
            resourceDescriptors[mipmap].UpdateBinding("u_prefilter", prefilter.BindingInfo(
                ImageFilter::Linear, ImageLayout::General, viewHandles[mipmap]));
            resourceDescriptors[mipmap].BindCompute(cmd, allocators, pipeline.GetLayout());
            
            struct PushConstants
            {
                glm::vec2 PrefilterResolutionInverse{};
                glm::vec2 EnvironmentResolutionInverse{};
                f32 Roughness{};
            };
            PushConstants pushConstants = {
                .PrefilterResolutionInverse = 1.0f / glm::vec2{(f32)resolution},
                .EnvironmentResolutionInverse = 1.0f / glm::vec2{
                    (f32)source.Description().Width, (f32)source.Description().Height},
                .Roughness = (f32)mipmap / (f32)prefilter.Description().Mipmaps};

            RenderCommand::PushConstants(cmd, pipeline.GetLayout(), pushConstants);
            RenderCommand::Dispatch(cmd,
                {resolution, resolution, 6},
                {32, 32, 1});

            RenderCommand::WaitOnBarrier(cmd, DependencyInfo::Builder().MemoryDependency({
                    .SourceStage = PipelineStage::ComputeShader,
                    .DestinationStage = PipelineStage::ComputeShader,
                    .SourceAccess = PipelineAccess::WriteShader,
                    .DestinationAccess = PipelineAccess::ReadShader})
                .SetFlags(PipelineDependencyFlags::ByRegion)
                .Build(deletionQueue));
            
            resolution = std::max(1u, resolution >> 1);
        }
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

    s_PendingTextures.clear();
}

Texture EnvironmentPrefilterProcessor::CreateEmptyTexture()
{
    u32 mipmapCount = Texture::CalculateMipmapCount({PREFILTER_RESOLUTION, PREFILTER_RESOLUTION});
    std::vector<ImageSubresourceDescription::Packed> additionalViews(mipmapCount);
    for (u32 i = 0; i < mipmapCount; i++)
        additionalViews[i] = ImageSubresourceDescription::Pack({
            .MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 6});

    return Texture::Builder({
            .Width = PREFILTER_RESOLUTION,
            .Height = PREFILTER_RESOLUTION,
            .Layers = 6,
            .Mipmaps = mipmapCount,
            .Format = Format::RGBA16_FLOAT,
            .Kind = ImageKind::Cubemap,
            .Usage = ImageUsage::Sampled | ImageUsage::Storage,
            .AdditionalViews = additionalViews})
        .Build();
}
