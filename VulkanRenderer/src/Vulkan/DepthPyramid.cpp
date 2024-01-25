#include "DepthPyramid.h"

#include <tracy/Tracy.hpp>

#include "Driver.h"
#include "RenderCommand.h"
#include "Renderer.h"
#include "VulkanCore.h"
#include "VulkanUtils.h"
#include "utils/MathUtils.h"
#include "utils/utils.h"


DepthPyramid::DepthPyramid(const Image& depthImage, const CommandBuffer& cmd,
    ComputeDepthPyramidData* computeDepthPyramidData)
    : m_ComputeDepthPyramidData(computeDepthPyramidData)
{
    m_Sampler = CreateSampler();

    m_PyramidDepth = CreatePyramidDepthImage(cmd, depthImage);
    m_MipMapViews = CreateViews(m_PyramidDepth);
    CreateDescriptorSets(depthImage);
}

DepthPyramid::~DepthPyramid()
{
    vkDestroySampler(Driver::DeviceHandle(), m_Sampler, nullptr);
    for (u32 i = 0; i < m_PyramidDepth.GetImageData().MipMapCount; i++)
    {
        vkDestroyImageView(Driver::DeviceHandle(), m_MipMapViews[i], nullptr);
        ShaderDescriptorSet::Destroy(m_DepthPyramidDescriptors[i]);
    }
    Image::Destroy(m_PyramidDepth);
}

void DepthPyramid::Compute(const Image& depthImage, const CommandBuffer& cmd)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Depth pyramid")
    Fill(cmd, depthImage);
}

VkSampler DepthPyramid::CreateSampler()
{
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = (f32)DepthPyramid::MAX_DEPTH;

    VkSamplerReductionModeCreateInfo reductionModeCreateInfo = {};
    reductionModeCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
    reductionModeCreateInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_MIN;
    samplerCreateInfo.pNext = &reductionModeCreateInfo;

    VkSampler sampler;
    
    VulkanCheck(vkCreateSampler(Driver::DeviceHandle(), &samplerCreateInfo, nullptr, &sampler),
        "Failed to create depth pyramid sampler");

    return sampler;
}

Image DepthPyramid::CreatePyramidDepthImage(const CommandBuffer& cmd, const Image& depthImage)
{
    u32 width = utils::floorToPowerOf2(depthImage.GetImageData().Width);
    u32 height = utils::floorToPowerOf2(depthImage.GetImageData().Height);
    Image pyramidImage = Image::Builder()
        .SetExtent({width, height})
        .SetFormat(VK_FORMAT_R32_SFLOAT)
        .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT)
        .CreateMipmaps(true, VK_FILTER_LINEAR)
        .CreateView(false)
        .BuildManualLifetime();

    pyramidImage.GetImageData().View = vkUtils::createImageView(Driver::DeviceHandle(),
        pyramidImage.GetImageData().Image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
        pyramidImage.GetImageData().MipMapCount);

    ImageSubresource imageSubresource = pyramidImage.CreateSubresource();
    Barrier barrier = {};

    DependencyInfo layoutTransition = DependencyInfo::Builder()
        .SetFlags(VK_DEPENDENCY_BY_REGION_BIT)
        .LayoutTransition({
            .ImageSubresource = &imageSubresource,
            .SourceStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .DestinationStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .SourceAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
            .DestinationAccess = VK_ACCESS_2_SHADER_READ_BIT,
            .OldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .NewLayout = VK_IMAGE_LAYOUT_GENERAL})
        .Build();
    
    barrier.Wait(cmd, layoutTransition);
    
    return pyramidImage;
}

std::array<VkImageView, DepthPyramid::MAX_DEPTH> DepthPyramid::CreateViews(const Image& pyramidImage)
{
    std::array<VkImageView, DepthPyramid::MAX_DEPTH> views;
    
    for (u32 i = 0; i < pyramidImage.GetImageData().MipMapCount; i++)
    {
        VkImageView view = vkUtils::createImageView(Driver::DeviceHandle(),
            pyramidImage.GetImageData().Image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
            1, i);

        views[i] = view;
    }

    return views;
}

void DepthPyramid::CreateDescriptorSets(const Image& depthImage)
{
    u32 mipMapCount = m_PyramidDepth.GetImageData().MipMapCount;
    for (u32 i = 0; i < mipMapCount; i++)
    {
        DescriptorSet::TextureBindingInfo destination = {
            .View = m_MipMapViews[i],
            .Sampler = m_Sampler,
            .Layout = VK_IMAGE_LAYOUT_GENERAL};

        DescriptorSet::TextureBindingInfo source = {
            .View = i > 0 ? m_MipMapViews[i - 1] : depthImage.GetImageData().View,
            .Sampler = m_Sampler,
            .Layout = i > 0 ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        
        m_DepthPyramidDescriptors[i] = ShaderDescriptorSet::Builder()
            .SetTemplate(&m_ComputeDepthPyramidData->PipelineTemplate)
            .AddBinding("u_in_image", source)
            .AddBinding("u_out_image", destination)
            .BuildManualLifetime();
    }
}

void DepthPyramid::Fill(const CommandBuffer& cmd, const Image& depthImage)
{
    TracyVkZone(ProfilerContext::Get()->GraphicsContext(), Driver::GetProfilerCommandBuffer(ProfilerContext::Get()), "Fill depth pyramid")

    ImageSubresource depthSubresource = depthImage.CreateSubresource();
    Barrier barrier = {};

    DependencyInfo depthToReadTransition = DependencyInfo::Builder()
        .SetFlags(VK_DEPENDENCY_BY_REGION_BIT)
        .LayoutTransition({
            .ImageSubresource = &depthSubresource,
            .SourceStage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .DestinationStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .SourceAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .DestinationAccess = VK_ACCESS_2_SHADER_READ_BIT,
            .OldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .NewLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})
        .Build();
    barrier.Wait(cmd, depthToReadTransition);

    auto* pipeline = &m_ComputeDepthPyramidData->Pipeline;
    pipeline->BindCompute(cmd);
    u32 mipMapCount = m_PyramidDepth.GetImageData().MipMapCount;
    u32 width = m_PyramidDepth.GetImageData().Width;  
    u32 height = m_PyramidDepth.GetImageData().Height;  
    for (u32 i = 0; i < mipMapCount; i++)
    {

        ShaderDescriptorSet& descriptorSet = m_DepthPyramidDescriptors[i];

        u32 levelWidth = std::max(1u, width >> i);
        u32 levelHeight = std::max(1u, height >> i);
        glm::uvec2 levels = {levelWidth, levelHeight};
        PushConstantDescription pushConstantDescription = PushConstantDescription::Builder()
            .SetStages(VK_SHADER_STAGE_COMPUTE_BIT)
            .SetSizeBytes(sizeof(glm::uvec2))
            .Build();

        descriptorSet.BindCompute(cmd, DescriptorKind::Global, pipeline->GetPipelineLayout());
        RenderCommand::PushConstants(cmd, pipeline->GetPipelineLayout(), &levels, pushConstantDescription);
        RenderCommand::Dispatch(cmd, {(levelWidth + 32 - 1) / 32, (levelHeight + 32 - 1) / 32, 1});

        ImageSubresource pyramidSubresource = m_PyramidDepth.CreateSubresource();
        pyramidSubresource.MipMapBase = i;
        pyramidSubresource.MipMapCount = 1;

        DependencyInfo pyramidTransition = DependencyInfo::Builder()
            .SetFlags(VK_DEPENDENCY_BY_REGION_BIT)
            .LayoutTransition({
                .ImageSubresource = &pyramidSubresource,
                .SourceStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DestinationStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .SourceAccess = VK_ACCESS_2_SHADER_WRITE_BIT,
                .DestinationAccess = VK_ACCESS_2_SHADER_READ_BIT,
                .OldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .NewLayout = VK_IMAGE_LAYOUT_GENERAL})
            .Build();
        barrier.Wait(cmd, pyramidTransition);
    }

    DependencyInfo readToDepthTransition = DependencyInfo::Builder()
       .SetFlags(VK_DEPENDENCY_BY_REGION_BIT)
       .LayoutTransition({
           .ImageSubresource = &depthSubresource,
           .SourceStage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
           .DestinationStage =  VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
               VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
           .SourceAccess = VK_ACCESS_2_SHADER_READ_BIT,
           .DestinationAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
           .OldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
           .NewLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL})
       .Build();
    barrier.Wait(cmd, readToDepthTransition);
}
