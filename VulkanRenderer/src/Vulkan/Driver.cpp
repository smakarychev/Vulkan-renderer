#include "Driver.h"

#include "core.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "Buffer.h"

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

void Driver::Unpack(const AttachmentTemplate& attachment, Subpass::Builder::CreateInfo& subpassCreateInfo)
{
    u32 attachmentIndex = (u32)subpassCreateInfo.Attachments.size();

    subpassCreateInfo.Attachments.push_back(attachment.m_AttachmentDescription);

    VkAttachmentReference attachmentReference = {};
    attachmentReference.attachment = attachmentIndex;
    attachmentReference.layout = attachment.m_AttachmentReferenceLayout;

    switch (attachment.m_Type)
    {
    case AttachmentType::Presentation:
    case AttachmentType::Color:
        subpassCreateInfo.ColorReferences.push_back(attachmentReference);
        break;
    case AttachmentType::DepthStencil:
        subpassCreateInfo.DepthStencilReference = attachmentReference;
        break;
    default:
        ASSERT(false, "Unknown attachment type")
        std::unreachable();
    }
    
}

void Driver::Unpack(const Subpass& subpass, RenderPass::Builder::CreateInfo& renderPassCreateInfo)
{
    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    // todo: fix when resolve attachments will be implemented
    subpassDescription.colorAttachmentCount = (u32)subpass.m_ColorReferences.size();
    subpassDescription.pColorAttachments = subpass.m_ColorReferences.data();
    
    subpassDescription.pDepthStencilAttachment = subpass.m_DepthStencilReference.has_value() ?
        subpass.m_DepthStencilReference.operator->() : nullptr; // thx committee
    
    renderPassCreateInfo.Subpasses.push_back(subpassDescription);

    renderPassCreateInfo.Attachments.append_range(subpass.m_Attachments);
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

void Driver::Unpack(const RenderPass& renderPass, Pipeline::Builder::CreateInfo& pipelineCreateInfo)
{
    pipelineCreateInfo.RenderPass = renderPass.m_RenderPass;
}

void Driver::Unpack(const Attachment& attachment, Framebuffer::Builder::CreateInfo& framebufferCreateInfo)
{
    if (framebufferCreateInfo.Attachments.empty())
    {
        framebufferCreateInfo.Width = attachment.m_ImageData.Width;
        framebufferCreateInfo.Height = attachment.m_ImageData.Height;
    }
    else
    {
        ASSERT(framebufferCreateInfo.Width == attachment.m_ImageData.Width &&
               framebufferCreateInfo.Height == attachment.m_ImageData.Height,
               "All attachments must be of equal dimensions")
    }
    
    framebufferCreateInfo.Attachments.push_back(attachment.m_ImageData.View);
}

void Driver::Unpack(const RenderPass& renderPass, Framebuffer::Builder::CreateInfo& framebufferCreateInfo)
{
    framebufferCreateInfo.RenderPass = renderPass.m_RenderPass;
}

void Driver::Unpack(const CommandPool& commandPool, CommandBuffer::Builder::CreateInfo& commandBufferCreateInfo)
{
    commandBufferCreateInfo.CommandPool = commandPool.m_CommandPool;
}

void Driver::Unpack(DescriptorAllocator& allocator, const DescriptorSetLayout& layout,
    DescriptorAllocator::SetAllocateInfo& setAllocateInfo)
{
    auto& info = setAllocateInfo.Info;
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = allocator.GrabPool();
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout.m_Layout;
}

void Driver::DescriptorSetBindBuffer(u32 slot, const DescriptorSet::BufferBindingInfo& bindingInfo,
                                     VkDescriptorType descriptor, VkShaderStageFlags stages, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    DescriptorAddBinding(slot, descriptor, stages, descriptorSetCreateInfo);
    
    VkDescriptorBufferInfo descriptorBufferInfo = {};
    descriptorBufferInfo.buffer = bindingInfo.Buffer->m_Buffer;
    descriptorBufferInfo.offset = bindingInfo.OffsetBytes;
    descriptorBufferInfo.range = bindingInfo.SizeBytes;

    descriptorSetCreateInfo.BoundBuffers.push_back({.ResourceInfo = descriptorBufferInfo, .Slot = slot});
}

void Driver::DescriptorSetBindTexture(u32 slot, const Texture& texture, VkDescriptorType descriptor,
    VkShaderStageFlags stages, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    DescriptorAddBinding(slot, descriptor, stages, descriptorSetCreateInfo);
    
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = Texture::CreateSampler(VK_FILTER_LINEAR); // todo: find a better place for it
    descriptorTextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorTextureInfo.imageView = texture.m_ImageData.View;

    descriptorSetCreateInfo.BoundTextures.push_back({.ResourceInfo = descriptorTextureInfo, .Slot = slot});
}

void Driver::DescriptorAddBinding(u32 slot, VkDescriptorType descriptor, VkShaderStageFlags stages,
    DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = slot;
    binding.descriptorType = descriptor;
    binding.descriptorCount = 1;
    binding.stageFlags = stages;

    descriptorSetCreateInfo.Bindings.push_back(binding);
}

void Driver::Init(const Device& device)
{
    s_State.Device = &device;
    
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.instance = device.m_Instance;
    createInfo.physicalDevice = device.m_GPU;
    createInfo.device = device.m_Device;
    vmaCreateAllocator(&createInfo, &s_State.Allocator);
    s_State.DeletionQueue.AddDeleter([](){ vmaDestroyAllocator(s_State.Allocator); });

    s_State.UploadContext.CommandPool = CommandPool::Builder().
        SetQueue(QueueKind::Graphics).
        Build();
    s_State.UploadContext.CommandBuffer = s_State.UploadContext.CommandPool.AllocateBuffer(CommandBufferKind::Primary);
    s_State.UploadContext.Fence = Fence::Builder().Build();
    s_State.UploadContext.QueueInfo = s_State.Device->GetQueues().Graphics;
}

void Driver::Shutdown()
{
    vkDeviceWaitIdle(DeviceHandle());
    s_State.DeletionQueue.Flush();
}
