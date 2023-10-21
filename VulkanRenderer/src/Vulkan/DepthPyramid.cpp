#include "DepthPyramid.h"

#include "Driver.h"
#include "RenderCommand.h"
#include "VulkanCore.h"
#include "VulkanUtils.h"

DepthPyramid::DepthPyramid(const Image& depthImage, const CommandBuffer& cmd,
        ShaderPipeline* depthPyramidPipeline, ShaderPipelineTemplate* depthPyramidTemplate)
{
    m_Sampler = CreateSampler();

    m_PyramidDepth = CreatePyramidDepthImage(cmd, depthImage);
    m_MipMapViews = CreateViews(m_PyramidDepth);
    CreateDescriptorSets(depthImage, depthPyramidTemplate);
    m_Pipeline = depthPyramidPipeline;
}

DepthPyramid::~DepthPyramid()
{
    vkDestroySampler(Driver::DeviceHandle(), m_Sampler, nullptr);
    for (u32 i = 0; i < m_PyramidDepth.GetImageData().MipMapCount; i++)
    {
        vkDestroyImageView(Driver::DeviceHandle(), m_MipMapViews[i], nullptr);
        ShaderDescriptorSet::Destroy(m_DescriptorSets[i]);
    }
    Image::Destroy(m_PyramidDepth);
}

void DepthPyramid::ComputePyramid(const Image& depthImage, const CommandBuffer& cmd)
{
    FillPyramid(cmd, depthImage);
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
    Image pyramidImage = Image::Builder()
        .SetExtent({depthImage.GetImageData().Width, depthImage.GetImageData().Height})
        .SetFormat(VK_FORMAT_R32_SFLOAT)
        .SetUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 0)
        .CreateMipmaps(true)
        .CreateView(false)
        .BuildManualLifetime();

    pyramidImage.GetImageData().View = vkUtils::createImageView(Driver::DeviceHandle(),
        pyramidImage.GetImageData().Image, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT,
        pyramidImage.GetImageData().MipMapCount);
    
    PipelineImageBarrierInfo barrierInfo = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .DependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .Image = &pyramidImage,
        .ImageSourceMask = VK_ACCESS_SHADER_WRITE_BIT,
        .ImageDestinationMask = VK_ACCESS_SHADER_READ_BIT,
        .ImageSourceLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .ImageDestinationLayout = VK_IMAGE_LAYOUT_GENERAL,
        .ImageAspect = VK_IMAGE_ASPECT_COLOR_BIT};
    RenderCommand::CreateBarrier(cmd, barrierInfo);
    
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

void DepthPyramid::CreateDescriptorSets(const Image& depthImage, ShaderPipelineTemplate* depthPyramidTemplate)
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
        
        m_DescriptorSets[i] = ShaderDescriptorSet::Builder()
            .SetTemplate(depthPyramidTemplate)
            .AddBinding("u_in_image", source)
            .AddBinding("u_out_image", destination)
            .BuildManualLifetime();
    }
}

void DepthPyramid::FillPyramid(const CommandBuffer& cmd, const Image& depthImage)
{
    PipelineImageBarrierInfo depthImageBarrier = {
        .PipelineSourceMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        .DependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        .Image = &depthImage,
        .ImageSourceMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .ImageDestinationMask = VK_ACCESS_SHADER_READ_BIT,
        .ImageSourceLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .ImageDestinationLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .ImageAspect = VK_IMAGE_ASPECT_DEPTH_BIT};
    RenderCommand::CreateBarrier(cmd, depthImageBarrier);
    
    m_Pipeline->Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
    u32 mipMapCount = m_PyramidDepth.GetImageData().MipMapCount;
    u32 width = m_PyramidDepth.GetImageData().Width;  
    u32 height = m_PyramidDepth.GetImageData().Height;  
    for (u32 i = 0; i < mipMapCount; i++)
    {
        ShaderDescriptorSet& descriptorSet = m_DescriptorSets[i];

        u32 levelWidth = std::max(1u, width >> i);
        u32 levelHeight = std::max(1u, height >> i);
        glm::uvec2 levels = {levelWidth, levelHeight};
        PushConstantDescription pushConstantDescription = PushConstantDescription::Builder()
            .SetStages(VK_SHADER_STAGE_COMPUTE_BIT)
            .SetSizeBytes(sizeof(glm::uvec2))
            .Build();

        descriptorSet.Bind(cmd, DescriptorKind::Global, m_Pipeline->GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE);
        RenderCommand::PushConstants(cmd, m_Pipeline->GetPipelineLayout(), &levels, pushConstantDescription);
        RenderCommand::Dispatch(cmd, {(levelWidth + 32 - 1) / 32, (levelHeight + 32 - 1) / 32, 1});
        PipelineImageBarrierInfo barrierInfo = {
            .PipelineSourceMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .PipelineDestinationMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            .DependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            .Image = &m_PyramidDepth,
            .ImageSourceMask = VK_ACCESS_SHADER_WRITE_BIT,
            .ImageDestinationMask = VK_ACCESS_SHADER_READ_BIT,
            .ImageSourceLayout = VK_IMAGE_LAYOUT_GENERAL,
            .ImageDestinationLayout = VK_IMAGE_LAYOUT_GENERAL,
            .ImageAspect = VK_IMAGE_ASPECT_COLOR_BIT};
        RenderCommand::CreateBarrier(cmd, barrierInfo);
    }
}
