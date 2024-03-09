#include "DepthPyramid.h"

#include <tracy/Tracy.hpp>

#include "Renderer.h"
#include "utils/MathUtils.h"
#include "utils/utils.h"
#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"


DepthPyramid::DepthPyramid(const Image& depthImage, const CommandBuffer& cmd,
                           ComputeDepthPyramidData* computeDepthPyramidData)
    : m_ComputeDepthPyramidData(computeDepthPyramidData)
{
    m_Sampler = CreateSampler();

    m_PyramidDepth = CreatePyramidDepthImage(cmd, depthImage);
    CreateDescriptorSets(depthImage);
}

DepthPyramid::~DepthPyramid()
{
    Image::Destroy(m_PyramidDepth);
}

void DepthPyramid::Compute(const Image& depthImage, const CommandBuffer& cmd, DeletionQueue& deletionQueue)
{
    GPU_PROFILE_FRAME("Depth pyramid")
    Fill(cmd, depthImage, deletionQueue);
}

Sampler DepthPyramid::CreateSampler()
{
    Sampler sampler = Sampler::Builder()
        .Filters(ImageFilter::Linear, ImageFilter::Linear)
        .MaxLod((f32)DepthPyramid::MAX_MIPMAP_COUNT)
        .WithAnisotropy(false)
        .ReductionMode(SamplerReductionMode::Min)
        .Build();
    
    return sampler;
}

Image DepthPyramid::CreatePyramidDepthImage(const CommandBuffer& cmd, const Image& depthImage)
{
    u32 width = utils::floorToPowerOf2(depthImage.GetDescription().Width);
    u32 height = utils::floorToPowerOf2(depthImage.GetDescription().Height);

    Image::Builder pyramidImageBuilder = Image::Builder()
        .SetExtent({width, height})
        .SetFormat(Format::R32_FLOAT)
        .SetUsage(ImageUsage::Sampled | ImageUsage::Storage)
        .CreateMipmaps(true, ImageFilter::Linear);

    u32 mipmapCount = Image::CalculateMipmapCount({width, height});

    for (u32 i = 0; i < mipmapCount; i++)
        pyramidImageBuilder.AddView(
            {.MipmapBase = i, .Mipmaps = 1, .LayerBase = 0, .Layers = 1}, m_MipmapViewHandles[i]);
    
    Image pyramidImage = pyramidImageBuilder.BuildManualLifetime();

    ImageSubresource imageSubresource = pyramidImage.CreateSubresource();
    Barrier barrier = {};
    DeletionQueue deletionQueue = {};
    
    DependencyInfo layoutTransition = DependencyInfo::Builder()
        .SetFlags(PipelineDependencyFlags::ByRegion)
        .LayoutTransition({
            .ImageSubresource = &imageSubresource,
            .SourceStage = PipelineStage::ComputeShader,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteShader,
            .DestinationAccess = PipelineAccess::ReadShader,
            .OldLayout = ImageLayout::Undefined,
            .NewLayout = ImageLayout::General})
        .Build(deletionQueue);
    
    barrier.Wait(cmd, layoutTransition);
    
    return pyramidImage;
}

void DepthPyramid::CreateDescriptorSets(const Image& depthImage)
{
    u32 mipMapCount = m_PyramidDepth.GetDescription().Mipmaps;
    for (u32 i = 0; i < mipMapCount; i++)
    {
        ImageBindingInfo source = i > 0 ?
            m_PyramidDepth.CreateBindingInfo(
                m_Sampler, ImageLayout::General, m_MipmapViewHandles[i - 1]) :
            depthImage.CreateBindingInfo(m_Sampler, ImageLayout::DepthReadonly);
        
        ImageBindingInfo destination = m_PyramidDepth.CreateBindingInfo(
            m_Sampler, ImageLayout::General, m_MipmapViewHandles[i]);
        
        m_DepthPyramidDescriptors[i] = ShaderDescriptorSet::Builder()
            .SetTemplate(m_ComputeDepthPyramidData->PipelineTemplate)
            .AddBinding("u_in_image", source)
            .AddBinding("u_out_image", destination)
            .Build();
    }
}

void DepthPyramid::Fill(const CommandBuffer& cmd, const Image& depthImage, DeletionQueue& deletionQueue)
{
    GPU_PROFILE_FRAME("Fill depth pyramid")

    ImageSubresource depthSubresource = depthImage.CreateSubresource();
    Barrier barrier = {};

    DependencyInfo depthToReadTransition = DependencyInfo::Builder()
        .SetFlags(PipelineDependencyFlags::ByRegion)
        .LayoutTransition({
            .ImageSubresource = &depthSubresource,
            .SourceStage = PipelineStage::DepthEarly | PipelineStage::DepthLate,
            .DestinationStage = PipelineStage::ComputeShader,
            .SourceAccess = PipelineAccess::WriteDepthStencilAttachment,
            .DestinationAccess = PipelineAccess::ReadShader,
            .OldLayout = ImageLayout::DepthAttachment,
            .NewLayout = ImageLayout::DepthReadonly})
        .Build(deletionQueue);
    barrier.Wait(cmd, depthToReadTransition);

    auto* pipeline = &m_ComputeDepthPyramidData->Pipeline;
    pipeline->BindCompute(cmd);
    u32 mipMapCount = m_PyramidDepth.GetDescription().Mipmaps;
    u32 width = m_PyramidDepth.GetDescription().Width;  
    u32 height = m_PyramidDepth.GetDescription().Height;  
    for (u32 i = 0; i < mipMapCount; i++)
    {

        ShaderDescriptorSet& descriptorSet = m_DepthPyramidDescriptors[i];

        u32 levelWidth = std::max(1u, width >> i);
        u32 levelHeight = std::max(1u, height >> i);
        glm::uvec2 levels = {levelWidth, levelHeight};

        descriptorSet.BindCompute(cmd, DescriptorKind::Global, pipeline->GetLayout());
        RenderCommand::PushConstants(cmd, pipeline->GetLayout(), &levels);
        RenderCommand::Dispatch(cmd, {(levelWidth + 32 - 1) / 32, (levelHeight + 32 - 1) / 32, 1});

        ImageSubresource pyramidSubresource = m_PyramidDepth.CreateSubresource();
        pyramidSubresource.Description.MipmapBase = i;
        pyramidSubresource.Description.Mipmaps = 1;

        DependencyInfo pyramidTransition = DependencyInfo::Builder()
            .SetFlags(PipelineDependencyFlags::ByRegion)
            .LayoutTransition({
                .ImageSubresource = &pyramidSubresource,
                .SourceStage = PipelineStage::ComputeShader,
                .DestinationStage = PipelineStage::ComputeShader,
                .SourceAccess = PipelineAccess::WriteShader,
                .DestinationAccess = PipelineAccess::ReadShader,
                .OldLayout = ImageLayout::General,
                .NewLayout = ImageLayout::General})
            .Build(deletionQueue);
        barrier.Wait(cmd, pyramidTransition);
    }

    DependencyInfo readToDepthTransition = DependencyInfo::Builder()
       .SetFlags(PipelineDependencyFlags::ByRegion)
       .LayoutTransition({
           .ImageSubresource = &depthSubresource,
           .SourceStage = PipelineStage::ComputeShader,
           .DestinationStage =  PipelineStage::DepthEarly | PipelineStage::DepthLate,
           .SourceAccess = PipelineAccess::ReadShader,
           .DestinationAccess =
               PipelineAccess::ReadDepthStencilAttachment | PipelineAccess::WriteDepthStencilAttachment,
           .OldLayout = ImageLayout::DepthReadonly,
           .NewLayout = ImageLayout::DepthAttachment})
       .Build(deletionQueue);
    barrier.Wait(cmd, readToDepthTransition);
}
