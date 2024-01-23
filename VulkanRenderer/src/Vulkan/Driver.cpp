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

void Driver::Unpack(const RenderingAttachment& attachment, RenderingInfo::Builder::CreateInfo& renderingInfoCreateInfo)
{
    if (attachment.m_Type == RenderingAttachmentType::Color)
        renderingInfoCreateInfo.ColorAttachments.push_back(attachment.m_AttachmentInfo);
    else
        renderingInfoCreateInfo.DepthAttachment = attachment.m_AttachmentInfo;
}

void Driver::Unpack(const PushConstantDescription& description, PipelineLayout::Builder::CreateInfo& pipelineLayoutCreateInfo)
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
    VkImageMemoryBarrier2 imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageMemoryBarrier.srcStageMask = layoutTransitionInfo.SourceStage;
    imageMemoryBarrier.dstStageMask = layoutTransitionInfo.DestinationStage;
    imageMemoryBarrier.srcAccessMask = layoutTransitionInfo.SourceAccess;
    imageMemoryBarrier.dstAccessMask = layoutTransitionInfo.DestinationAccess;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.oldLayout = layoutTransitionInfo.OldLayout;
    imageMemoryBarrier.newLayout = layoutTransitionInfo.NewLayout;
    imageMemoryBarrier.image = layoutTransitionInfo.ImageSubresource->Image->m_ImageData.Image;
    imageMemoryBarrier.subresourceRange = {
        .aspectMask = layoutTransitionInfo.ImageSubresource->Aspect,
        .baseMipLevel = layoutTransitionInfo.ImageSubresource->MipMapBase,
        .levelCount = layoutTransitionInfo.ImageSubresource->MipMapCount,
        .baseArrayLayer = layoutTransitionInfo.ImageSubresource->LayerBase,
        .layerCount = layoutTransitionInfo.ImageSubresource->LayerCount};

    createInfo.LayoutTransitionInfo = imageMemoryBarrier;
}

void Driver::DescriptorSetBindBuffer(u32 slot, const DescriptorSet::BufferBindingInfo& bindingInfo,
                                     VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = bindingInfo.Buffer->m_Buffer;
    descriptorBufferInfo.offset = bindingInfo.OffsetBytes;
    descriptorBufferInfo.range = bindingInfo.SizeBytes;

    descriptorSetCreateInfo.BoundResources.push_back({.Buffer = descriptorBufferInfo, .Slot = slot, .Type = descriptor});
}

void Driver::DescriptorSetBindTexture(u32 slot, const Texture& texture, VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    TextureDescriptorInfo descriptorInfo = texture.CreateDescriptorInfo(VK_FILTER_LINEAR);
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = descriptorInfo.Sampler;
    descriptorTextureInfo.imageLayout = descriptorInfo.Layout;
    descriptorTextureInfo.imageView = descriptorInfo.View;

    descriptorSetCreateInfo.BoundResources.push_back({.Texture = descriptorTextureInfo, .Slot = slot, .Type = descriptor});
}

void Driver::DescriptorSetBindTexture(u32 slot, const DescriptorSet::TextureBindingInfo& texture,
    VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = texture.Sampler;
    descriptorTextureInfo.imageView = texture.View;
    descriptorTextureInfo.imageLayout = texture.Layout;

    descriptorSetCreateInfo.BoundResources.push_back({.Texture = descriptorTextureInfo, .Slot = slot, .Type = descriptor});
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
    TracyVkCtx context = TracyVkContext(GetDevice().m_GPU, GetDevice().m_Device, GetDevice().GetQueues().Graphics.Queue, cmd.m_CommandBuffer)
    return context;
}

void Driver::DestroyTracyGraphicsContext(TracyVkCtx context)
{
    TracyVkDestroy(context)
}

VkSampler* Driver::GetImmutableSampler()
{
    static VkSampler sampler = Texture::CreateSampler(VK_FILTER_LINEAR, VK_LOD_CLAMP_NONE);

    return &sampler;
}

VkCommandBuffer Driver::GetProfilerCommandBuffer(ProfilerContext* context)
{
    return context->m_GraphicsCommandBuffers[context->m_CurrentFrame]->m_CommandBuffer;
}
