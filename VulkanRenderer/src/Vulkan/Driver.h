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

#include "DescriptorSet.h"
#include "Shader.h"
#include "UploadContext.h"


class ShaderPipeline;

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
    UploadContext UploadContext;
};

class Driver
{
public:
    static void Unpack(const Device& device, Swapchain::Builder::CreateInfo& swapchainCreateInfo);

    static void Unpack(const AttachmentTemplate& attachment, Subpass::Builder::CreateInfo& subpassCreateInfo);

    static void Unpack(const Subpass& subpass, RenderPass::Builder::CreateInfo& renderPassCreateInfo);

    static void Unpack(const PushConstantDescription& description, PipelineLayout::Builder::CreateInfo& pipelineLayoutCreateInfo);
    static void Unpack(const DescriptorSetLayout& layout, PipelineLayout::Builder::CreateInfo& pipelineLayoutCreateInfo);
    
    static void Unpack(const PipelineLayout& pipelineLayout, Pipeline::Builder::CreateInfo& pipelineCreateInfo);
    static void Unpack(const RenderPass& renderPass, Pipeline::Builder::CreateInfo& pipelineCreateInfo);

    static void Unpack(const Attachment& attachment, Framebuffer::Builder::CreateInfo& framebufferCreateInfo);
    static void Unpack(const RenderPass& renderPass, Framebuffer::Builder::CreateInfo& framebufferCreateInfo);

    static void Unpack(const CommandPool& commandPool, CommandBuffer::Builder::CreateInfo& commandBufferCreateInfo);

    static void Unpack(DescriptorAllocator& allocator, const DescriptorSetLayout& layout, DescriptorAllocator::SetAllocateInfo& setAllocateInfo);

    static void DescriptorSetBindBuffer(u32 slot, const DescriptorSet::BufferBindingInfo& bindingInfo,
        VkDescriptorType descriptor, VkShaderStageFlags stages, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);
    static void DescriptorSetBindTexture(u32 slot, const Texture& texture,
        VkDescriptorType descriptor, VkShaderStageFlags stages, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);

    template <typename Fn>
    static void ImmediateUpload(Fn&& uploadFunction);
    
    static void Init(const Device& device);
    static void Shutdown();

    static VkDevice DeviceHandle() { return s_State.Device->m_Device; }
    static const Device& GetDevice() { return *s_State.Device; }
    static DriverDeletionQueue& DeletionQueue() { return s_State.DeletionQueue; }
    static VmaAllocator& Allocator() { return s_State.Allocator; }
    static u64 GetUniformBufferAlignment() { return s_State.Device->m_GPUProperties.limits.minUniformBufferOffsetAlignment; }
    static f32 GetAnisotropyLevel() { return s_State.Device->m_GPUProperties.limits.maxSamplerAnisotropy; }
    static UploadContext* UploadContext() { return &s_State.UploadContext; }
private:
    static void DescriptorAddBinding(u32 slot, VkDescriptorType descriptor, VkShaderStageFlags stages,
        DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);
public:
    static DriverState s_State;
};

template <typename Fn>
void Driver::ImmediateUpload(Fn&& uploadFunction)
{
    auto&& [pool, cmd, fence, queue] = *UploadContext();
    
    cmd.Begin();

    
    uploadFunction(cmd);

    
    cmd.End();
    cmd.Submit(queue, fence);
    fence.Wait();
    fence.Reset();
    pool.Reset();
}
