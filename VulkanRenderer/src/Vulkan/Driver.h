#pragma once

#include <functional>

#include "CommandBuffer.h"
#include "Device.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Syncronization.h"

#include <vma/vk_mem_alloc.h>

#include "DescriptorSet.h"
#include "UploadContext.h"

#include <tracy/TracyVulkan.hpp>

#include "RenderingInfo.h"


class ProfilerContext;
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

    static void Unpack(const RenderingAttachment& attachment, RenderingInfo::Builder::CreateInfo& renderingInfoCreateInfo);
    
    static void Unpack(const PushConstantDescription& description, PipelineLayout::Builder::CreateInfo& pipelineLayoutCreateInfo);
    static void Unpack(const DescriptorSetLayout& layout, PipelineLayout::Builder::CreateInfo& pipelineLayoutCreateInfo);
    
    static void Unpack(const PipelineLayout& pipelineLayout, Pipeline::Builder::CreateInfo& pipelineCreateInfo);

    static void Unpack(const CommandPool& commandPool, CommandBuffer::Builder::CreateInfo& commandBufferCreateInfo);

    static void Unpack(DescriptorAllocator::PoolInfo pool, const DescriptorSetLayout& layout,
        DescriptorAllocator::SetAllocateInfo& setAllocateInfo);

    static void DescriptorSetBindBuffer(u32 slot, const DescriptorSet::BufferBindingInfo& bindingInfo,
        VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);
    static void DescriptorSetBindTexture(u32 slot, const Texture& texture,
        VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);
    static void DescriptorSetBindTexture(u32 slot, const DescriptorSet::TextureBindingInfo& texture,
        VkDescriptorType descriptor, DescriptorSet::Builder::CreateInfo& descriptorSetCreateInfo);

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
    static u32 GetMaxIndexingImages() { return s_State.Device->m_GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindSampledImages; }
    static u32 GetMaxIndexingUniformBuffers() { return s_State.Device->m_GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers; }
    static u32 GetMaxIndexingUniformBuffersDynamic() { return s_State.Device->m_GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindUniformBuffers; }
    static u32 GetMaxIndexingStorageBuffers() { return s_State.Device->m_GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic; }
    static u32 GetMaxIndexingStorageBuffersDynamic() { return s_State.Device->m_GPUDescriptorIndexingProperties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic; }
    static u32 GetSubgroupSize() { return s_State.Device->m_GPUSubgroupProperties.subgroupSize; }
    static UploadContext* UploadContext() { return &s_State.UploadContext; }

    static TracyVkCtx CreateTracyGraphicsContext(const CommandBuffer& cmd);
    static void DestroyTracyGraphicsContext(TracyVkCtx context);

    static VkSampler* GetImmutableSampler();

    static VkCommandBuffer GetProfilerCommandBuffer(ProfilerContext* context);
    
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
