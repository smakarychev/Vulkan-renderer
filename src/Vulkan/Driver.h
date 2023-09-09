#pragma once

#include <functional>

#include "Attachment.h"
#include "CommandBuffer.h"
#include "Device.h"
#include "Framebuffer.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "Swapchain.h"

#include <vma/vk_mem_alloc.h>

#include "DescriptorSet.h"


class DriverDeletionQueue
{
public:
    void AddDeleter(std::function<void()>&& deleter);
    void Flush();
private:
    std::vector<std::function<void()>> m_Deleters;
};

struct DriverState
{
    const Device* Device; 
    VmaAllocator Allocator;
    DriverDeletionQueue DeletionQueue;
};

class Driver
{
public:
    static void Unpack(const Device& device, Swapchain::Builder::CreateInfo& swapchainCreateInfo);

    static void Unpack(const AttachmentTemplate& attachment, Subpass::Builder::CreateInfo& subpassCreateInfo);

    static void Unpack(const Subpass& subpass, RenderPass::Builder::CreateInfo& renderPassCreateInfo);

    static void Unpack(const RenderPass& renderPass, Pipeline::Builder::CreateInfo& pipelineCreateInfo);
    static void Unpack(const PushConstantDescription& description, Pipeline::Builder::CreateInfo& pipelineCreateInfo);
    static void Unpack(const DescriptorSetLayout& layout, Pipeline::Builder::CreateInfo& pipelineCreateInfo);

    static void Unpack(const Attachment& attachment, Framebuffer::Builder::CreateInfo& framebufferCreateInfo);
    static void Unpack(const RenderPass& renderPass, Framebuffer::Builder::CreateInfo& framebufferCreateInfo);

    static void Unpack(const CommandPool& commandPool, CommandBuffer::Builder::CreateInfo& commandBufferCreateInfo);

    static void Unpack(const DescriptorPool& pool, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);
    static void Unpack(const DescriptorSetLayout& layout, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);

    static void DescriptorSetBindBuffer(const DescriptorSet& descriptorSet, u32 slot, const Buffer& buffer, u64 sizeBytes, u64 offset);
    
    static void Init(const Device& device);
    static void Shutdown();

    static VkDevice DeviceHandle() { return s_State.Device->m_Device; }
    static const Device& GetDevice() { return *s_State.Device; }
    static DriverDeletionQueue& DeletionQueue() { return s_State.DeletionQueue; }
    static VmaAllocator& Allocator() { return s_State.Allocator; }
    static u64 GetUniformBufferAlignment() { return s_State.Device->m_GPUProperties.limits.minUniformBufferOffsetAlignment; }
public:
    static DriverState s_State;
};
