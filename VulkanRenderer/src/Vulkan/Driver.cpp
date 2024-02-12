#include "Driver.h"

#include "Core/core.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

#include "Buffer.h"
#include "Core/ProfilerContext.h"

void DriverDeletionQueue::AddDeleter(std::function<void()>&& deleter)
{
    m_Deleters.push_back(deleter);
}

void DriverDeletionQueue::Flush()
{
    while(!m_Deleters.empty())
    {
        m_Deleters.back()();
        m_Deleters.pop_back();
    }
}

DriverState Driver::s_State = DriverState{};

void Driver::Unpack(const Device& device, Swapchain::Builder::CreateInfo& swapchainCreateInfo)
{
    swapchainCreateInfo.Window = device.m_Window;
    swapchainCreateInfo.Surface = device.m_Surface;
    swapchainCreateInfo.Queues = &device.m_Queues;
}

std::vector<Image> Driver::CreateSwapchainImages(const Swapchain& swapchain)
{
    u32 imageCount = 0;
    vkGetSwapchainImagesKHR(Driver::DeviceHandle(), swapchain.m_Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(Driver::DeviceHandle(), swapchain.m_Swapchain, &imageCount, images.data());

    ImageDescription description = {
        .Width = swapchain.m_Extent.width,
        .Height = swapchain.m_Extent.height,
        .Layers = 1,
        .Mipmaps = 1,
        .Kind = ImageKind::Image2d,
        .Usage = ImageUsage::Destination};
    std::vector<Image> colorImages(imageCount);
    for (auto& image : colorImages)
        image.m_Description = description;
    
    
    std::vector<VkImageView> imageViews(imageCount);
    for (u32 i = 0; i < imageCount; i++)
    {
        colorImages[i].m_Image = images[i];
        colorImages[i].m_View = Image::CreateVulkanImageView(
            colorImages[i].CreateSubresource(0, 1, 0, 1), swapchain.m_ColorFormat);
    }

    return colorImages;
}

void Driver::DestroySwapchainImages(const Swapchain& swapchain)
{
    for (const auto& colorImage : swapchain.m_ColorImages)
        vkDestroyImageView(Driver::DeviceHandle(), colorImage.m_View, nullptr);
}

void Driver::Unpack(const RenderingAttachment& attachment, RenderingInfo::Builder::CreateInfo& renderingInfoCreateInfo)
{
    if (attachment.m_Type == RenderingAttachmentType::Color)
        renderingInfoCreateInfo.ColorAttachments.push_back(attachment.m_AttachmentInfo);
    else
        renderingInfoCreateInfo.DepthAttachment = attachment.m_AttachmentInfo;
}

void Driver::Unpack(const Image& image, ImageLayout layout,
    RenderingAttachment::Builder::CreateInfo& renderAttachmentCreateInfo)
{
    VkRenderingAttachmentInfo renderingAttachmentInfo = Image::CreateVulkanRenderingAttachment(image, layout);
    renderAttachmentCreateInfo.AttachmentInfo.imageView = renderingAttachmentInfo.imageView;
    renderAttachmentCreateInfo.AttachmentInfo.imageLayout = renderingAttachmentInfo.imageLayout;
}

void Driver::Unpack(const PushConstantDescription& description,
                    PipelineLayout::Builder::CreateInfo& pipelineLayoutCreateInfo)
{
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.size = description.m_SizeBytes;
    pushConstantRange.offset = description.m_Offset;
    pushConstantRange.stageFlags = description.m_StageFlags;
    
    pipelineLayoutCreateInfo.PushConstantRanges.push_back(pushConstantRange);
}

void Driver::Unpack(const DescriptorSetLayout& layout, PipelineLayout::Builder::CreateInfo& pipelineLayoutCreateInfo)
{
    pipelineLayoutCreateInfo.DescriptorSetLayouts.push_back(layout.m_Layout);
}

void Driver::Unpack(const PipelineLayout& pipelineLayout, Pipeline::Builder::CreateInfo& pipelineCreateInfo)
{
    pipelineCreateInfo.Layout = pipelineLayout.m_Layout;
}

void Driver::Unpack(const RenderingDetails& renderingDetails, Pipeline::Builder::CreateInfo& pipelineCreateInfo)
{
    pipelineCreateInfo.ColorFormats.resize(renderingDetails.ColorFormats.size());
    pipelineCreateInfo.PipelineRenderingCreateInfo = Image::CreateVulkanRenderingInfo(
        renderingDetails, pipelineCreateInfo.ColorFormats);
}

void Driver::Unpack(const CommandPool& commandPool, CommandBuffer::Builder::CreateInfo& commandBufferCreateInfo)
{
    commandBufferCreateInfo.CommandPool = commandPool.m_CommandPool;
}

void Driver::Unpack(DescriptorAllocator::PoolInfo pool, const DescriptorSetLayout& layout,
    DescriptorAllocator::SetAllocateInfo& setAllocateInfo)
{
    auto& info = setAllocateInfo.Info;
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool.Pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout.m_Layout;
}

void Driver::Unpack(const LayoutTransitionInfo& layoutTransitionInfo, DependencyInfo::Builder::CreateInfo& createInfo)
{
    Image::FillVulkanLayoutTransitionBarrier(layoutTransitionInfo, *createInfo.LayoutTransitionInfo); 
}

void Driver::DescriptorSetBindBuffer(u32 slot, const DescriptorSet::BufferBindingInfo& bindingInfo,
    VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = bindingInfo.Buffer->m_Buffer;
    descriptorBufferInfo.offset = bindingInfo.Offset;
    descriptorBufferInfo.range = bindingInfo.SizeBytes;

    descriptorSetCreateInfo.BoundResources.push_back({
        .Buffer = descriptorBufferInfo, .Slot = slot, .Type = descriptor});
}

void Driver::DescriptorSetBindTexture(u32 slot, const DescriptorSet::TextureBindingInfo& texture,
    VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    VkDescriptorImageInfo descriptorTextureInfo = Image::CreateVulkanImageDescriptor(texture);

    descriptorSetCreateInfo.BoundResources.push_back({
        .Texture = descriptorTextureInfo, .Slot = slot, .Type = descriptor});
}

void Driver::UpdateDescriptorSet(DescriptorSet& descriptorSet,
    u32 slot, const Texture& texture, VkDescriptorType descriptor, u32 arrayIndex)
{
    ImageBindingInfo bindingInfo = texture.CreateBindingInfo({}, ImageLayout::ReadOnly);
    VkDescriptorImageInfo descriptorTextureInfo = Image::CreateVulkanImageDescriptor(bindingInfo);

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;   
    write.dstSet = descriptorSet.m_DescriptorSet;
    write.descriptorType = descriptor;
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;
    write.dstArrayElement = arrayIndex;

    vkUpdateDescriptorSets(DeviceHandle(), 1, &write, 0, nullptr);
}

void Driver::Init(const Device& device)
{
    s_State.Device = &device;

    VmaVulkanFunctions vulkanFunctions;
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
    vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
    vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.instance = device.m_Instance;
    createInfo.physicalDevice = device.m_GPU;
    createInfo.device = device.m_Device;
    createInfo.pVulkanFunctions = (const VmaVulkanFunctions*)&vulkanFunctions;
    createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    
    vmaCreateAllocator(&createInfo, &s_State.Allocator);
    s_State.DeletionQueue.AddDeleter([](){ vmaDestroyAllocator(s_State.Allocator); });

    s_State.UploadContext.CommandPool = CommandPool::Builder()
        .SetQueue(QueueKind::Graphics)
        .Build();
    s_State.UploadContext.CommandBuffer = s_State.UploadContext.CommandPool.AllocateBuffer(CommandBufferKind::Primary);
    s_State.UploadContext.Fence = Fence::Builder().Build();
    s_State.UploadContext.QueueInfo = s_State.Device->GetQueues().Graphics;
}

void Driver::Shutdown()
{
    vkDeviceWaitIdle(DeviceHandle());
    s_State.DeletionQueue.Flush();
}

TracyVkCtx Driver::CreateTracyGraphicsContext(const CommandBuffer& cmd)
{
    TracyVkCtx context = TracyVkContext(GetDevice().m_GPU, GetDevice().m_Device,
        GetDevice().GetQueues().Graphics.Queue, cmd.m_CommandBuffer)
    return context;
}

void Driver::DestroyTracyGraphicsContext(TracyVkCtx context)
{
    TracyVkDestroy(context)
}

void Driver::AddImmutableSampler(ShaderPipelineTemplate::DescriptorsFlags& descriptorsFlags)
{
    static Sampler sampler = Sampler::Builder()
        .Filters(ImageFilter::Linear, ImageFilter::Linear)
        .Build();

    descriptorsFlags.Descriptors.back().pImmutableSamplers = &sampler.m_Sampler;
}

VkCommandBuffer Driver::GetProfilerCommandBuffer(ProfilerContext* context)
{
    return context->m_GraphicsCommandBuffers[context->m_CurrentFrame]->m_CommandBuffer;
}
