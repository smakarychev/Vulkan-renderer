#pragma once

#include <functional>

#include "Attachment.h"
#include "CommandBuffer.h"
#include "Device.h"
#include "Framebuffer.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "Swapchain.h"
#include "Syncronization.h"

#include <vma/vk_mem_alloc.h>

#include "Image.h"

class DriverDeletionQueue
{
public:
    void AddDeleter(const std::function<void()>& deleter);
    void Flush();
private:
    std::vector<std::function<void()>> m_Deleters;
};

class Driver
{
public:
    static void Unpack(const Device& device, Swapchain::Builder::CreateInfo& swapchainCreateInfo);

    static void Unpack(const AttachmentTemplate& attachment, Subpass::Builder::CreateInfo& subpassCreateInfo);

    static void Unpack(const Device& device, RenderPass::Builder::CreateInfo& renderPassCreateInfo);
    static void Unpack(const Subpass& subpass, RenderPass::Builder::CreateInfo& renderPassCreateInfo);

    static void Unpack(const RenderPass& renderPass, Pipeline::Builder::CreateInfo& pipelineCreateInfo);
    static void Unpack(const PushConstantDescription& description, Pipeline::Builder::CreateInfo& pipelineCreateInfo);

    static void Unpack(const Attachment& attachment, Framebuffer::Builder::CreateInfo& framebufferCreateInfo);
    static void Unpack(const RenderPass& renderPass, Framebuffer::Builder::CreateInfo& framebufferCreateInfo);

    static void Unpack(const Device& device, CommandPool::Builder::CreateInfo& commandPoolCreateInfo);

    static void Unpack(const CommandPool& commandPool, CommandBuffer::Builder::CreateInfo& commandBufferCreateInfo);

    static void Unpack(const Device& device, Fence::Builder::CreateInfo& fenceCreateInfo);
    static void Unpack(const Device& device, Semaphore::Builder::CreateInfo& semaphoreCreateInfo);

    static void Unpack(const Device& device, Image::Builder::CreateInfo& imageCreateInfo);
    static void Unpack(const Swapchain& swapchain, Image::Builder::CreateInfo& imageCreateInfo);

    static void Init(const Device& device);
    static void Shutdown(const Device device);
public:
    static DriverDeletionQueue s_DeletionQueue;
    static VmaAllocator s_Allocator;
};
