#include "Driver.h"

#include "core.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include "Buffer.h"

void DriverDeletionQueue::AddDeleter(const std::function<void()>& deleter)
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

DriverDeletionQueue Driver::s_DeletionQueue = DriverDeletionQueue{};
VmaAllocator Driver::s_Allocator = VmaAllocator{};

void Driver::Unpack(const Device& device, Swapchain::Builder::CreateInfo& swapchainCreateInfo)
{
    swapchainCreateInfo.Window = device.m_Window;
    swapchainCreateInfo.Device = device.m_Device;
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

void Driver::Unpack(const Device& device, RenderPass::Builder::CreateInfo& renderPassCreateInfo)
{
    renderPassCreateInfo.Device = device.m_Device;
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

void Driver::Unpack(const RenderPass& renderPass, Pipeline::Builder::CreateInfo& pipelineCreateInfo)
{
    pipelineCreateInfo.Device = renderPass.m_Device;
    pipelineCreateInfo.RenderPass = renderPass.m_RenderPass;
}

void Driver::Unpack(const PushConstantDescription& description, Pipeline::Builder::CreateInfo& pipelineCreateInfo)
{
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.offset = 0;
    pushConstantRange.size = description.m_SizeBytes;
    pushConstantRange.stageFlags = description.m_StageFlags;

    pipelineCreateInfo.PushConstantRanges.push_back(pushConstantRange);
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
    framebufferCreateInfo.Device = renderPass.m_Device;
}

void Driver::Unpack(const Device& device, CommandPool::Builder::CreateInfo& commandPoolCreateInfo)
{
    commandPoolCreateInfo.Device = device.m_Device;
}

void Driver::Unpack(const CommandPool& commandPool, CommandBuffer::Builder::CreateInfo& commandBufferCreateInfo)
{
    commandBufferCreateInfo.CommandPool = commandPool.m_CommandPool;
    commandBufferCreateInfo.Device = commandPool.m_Device;
}

void Driver::Unpack(const Device& device, Fence::Builder::CreateInfo& fenceCreateInfo)
{
    fenceCreateInfo.Device = device.m_Device;
}

void Driver::Unpack(const Device& device, Semaphore::Builder::CreateInfo& semaphoreCreateInfo)
{
    semaphoreCreateInfo.Device = device.m_Device;
}

void Driver::Unpack(const Device& device, Image::Builder::CreateInfo& imageCreateInfo)
{
    imageCreateInfo.Device = device.m_Device;
}

void Driver::Unpack(const Swapchain& swapchain, Image::Builder::CreateInfo& imageCreateInfo)
{
    imageCreateInfo.Device = swapchain.m_Device;
}

void Driver::Init(const Device& device)
{
    VmaAllocatorCreateInfo createInfo = {};
    createInfo.instance = device.m_Instance;
    createInfo.physicalDevice = device.m_GPU;
    createInfo.device = device.m_Device;
    
    vmaCreateAllocator(&createInfo, &s_Allocator);

    s_DeletionQueue.AddDeleter([](){ vmaDestroyAllocator(s_Allocator); });
}

void Driver::Shutdown(const Device device)
{
    vkDeviceWaitIdle(device.m_Device);
    s_DeletionQueue.Flush();
}
