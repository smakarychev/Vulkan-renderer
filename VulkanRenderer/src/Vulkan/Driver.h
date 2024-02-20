#pragma once

#include <functional>

#include "Rendering/CommandBuffer.h"
#include "Rendering/Device.h"
#include "Rendering/Pipeline.h"
#include "Rendering/Swapchain.h"
#include "Rendering/Synchronization.h"

#include <vma/vk_mem_alloc.h>

#include "Rendering/DescriptorSet.h"

#include <volk.h>
#include <tracy/TracyVulkan.hpp>

#include "Rendering/RenderingInfo.h"
#include "Core/Freelist.h"
#include "Core/ProfilerContext.h"


class ProfilerContext;
class ShaderPipeline;

struct UploadContext
{
    CommandPool CommandPool;
    CommandBuffer CommandBuffer;
    Fence Fence;
    QueueInfo QueueInfo;
};

class DriverResources
{
    FRIEND_INTERNAL
private:
    template <typename Resource>
    constexpr auto AddResource(Resource&& resource);
    template <typename ResourceList, typename Resource>
    constexpr auto AddToResourceList(ResourceList& list, Resource&& value);
    template <typename Type>
    constexpr void RemoveResource(const Type& type);
    template <typename Type>
    constexpr const auto& operator[](const Type& type) const;
    template <typename Type>
    constexpr auto& operator[](const Type& type);

    void MapCmdToPool(const CommandBuffer& cmd, const CommandPool& pool);
    void DestroyCmdsOfPool(const CommandPool& pool);
    
private:
    // this one is a little strange
    struct DeviceResource
    {
        using ObjectType = Device;
        VkInstance Instance{VK_NULL_HANDLE};
        VkSurfaceKHR Surface{VK_NULL_HANDLE};
        VkPhysicalDevice GPU{VK_NULL_HANDLE};
        VkDevice Device{VK_NULL_HANDLE};
        VkPhysicalDeviceProperties GPUProperties;
        VkPhysicalDeviceDescriptorIndexingProperties GPUDescriptorIndexingProperties;
        VkPhysicalDeviceSubgroupProperties GPUSubgroupProperties;
        VkDebugUtilsMessengerEXT DebugUtilsMessenger;
    };
    struct SwapchainResource
    {
        using ObjectType = Swapchain;
        VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
        VkFormat ColorFormat{};
    };
    struct BufferResource
    {
        using ObjectType = Buffer;
        VkBuffer Buffer{VK_NULL_HANDLE};
        VmaAllocation Allocation{VK_NULL_HANDLE};
    };
    struct ImageResource
    {
        using ObjectType = Image;
        VkImage Image{VK_NULL_HANDLE};
        VkImageView View{VK_NULL_HANDLE};
        VmaAllocation Allocation{VK_NULL_HANDLE};
    };
    struct SamplerResource
    {
        using ObjectType = Sampler;
        VkSampler Sampler{VK_NULL_HANDLE};
    };
    struct ViewListResource
    {
        using ObjectType = ImageViewList;
        std::vector<VkImageView> Views;
    };
    struct CommandPoolResource
    {
        using ObjectType = CommandPool;
        VkCommandPool CommandPool{VK_NULL_HANDLE};
    };
    struct CommandBufferResource
    {
        using ObjectType = CommandBuffer;
        VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
    };
    struct QueueResource
    {
        using ObjectType = QueueInfo;
        VkQueue Queue{VK_NULL_HANDLE};
    };
    struct DescriptorSetLayoutResource
    {
        using ObjectType = DescriptorSetLayout;
        VkDescriptorSetLayout Layout{VK_NULL_HANDLE};
    };
    struct DescriptorSetResource
    {
        using ObjectType = DescriptorSet;
        VkDescriptorSet DescriptorSet{VK_NULL_HANDLE};
        VkDescriptorPool Pool{VK_NULL_HANDLE};
    };
    struct DescriptorAllocatorResource
    {
        using ObjectType = DescriptorAllocator;
        struct PoolInfo
        {
            VkDescriptorPool Pool;
            DescriptorPoolFlags Flags;
            u32 AllocationCount{0};
        };
        std::vector<PoolInfo> FreePools;
        std::vector<PoolInfo> UsedPools;
    };
    struct PipelineLayoutResource
    {
        using ObjectType = PipelineLayout;
        VkPipelineLayout Layout{VK_NULL_HANDLE};
        std::vector<VkPushConstantRange> PushConstants;
    };
    struct PipelineResource
    {
        using ObjectType = Pipeline;
        VkPipeline Pipeline{VK_NULL_HANDLE};
    };
    struct RenderingAttachmentResource
    {
        using ObjectType = RenderingAttachment;
        VkRenderingAttachmentInfo AttachmentInfo{};
    };
    struct RenderingInfoResource
    {
        using ObjectType = RenderingInfo;
        std::vector<VkRenderingAttachmentInfo> ColorAttachments;
        std::optional<VkRenderingAttachmentInfo> DepthAttachment;
    };
    struct FenceResource
    {
        using ObjectType = Fence;
        VkFence Fence{VK_NULL_HANDLE};
    };
    struct SemaphoreResource
    {
        using ObjectType = Semaphore;
        VkSemaphore Semaphore{VK_NULL_HANDLE};
    };
    struct DependencyInfoResource
    {
        using ObjectType = DependencyInfo;
        VkDependencyInfo DependencyInfo;
        std::vector<VkMemoryBarrier2> ExecutionMemoryDependenciesInfo;
        std::vector<VkImageMemoryBarrier2> LayoutTransitionsInfo;
    };
    struct SplitBarrierResource
    {
        using ObjectType = SplitBarrier;
        VkEvent Event{VK_NULL_HANDLE};
    };
    struct ShaderModuleResource
    {
        using ObjectType = ShaderModule;
        VkShaderModule Shader{VK_NULL_HANDLE};
    };
    
    u32 m_AllocatedCount{0};
    u32 m_DeallocatedCount{0};
    
    Freelist<DeviceResource> m_Devices;
    Freelist<SwapchainResource> m_Swapchains;
    Freelist<BufferResource> m_Buffers;
    Freelist<ImageResource> m_Images;
    Freelist<SamplerResource> m_Samplers;
    Freelist<ViewListResource> m_ViewLists;
    Freelist<CommandPoolResource> m_CommandPools;
    Freelist<CommandBufferResource> m_CommandBuffers;
    Freelist<QueueResource> m_Queues;
    Freelist<DescriptorSetLayoutResource> m_DescriptorLayouts;
    Freelist<DescriptorSetResource> m_DescriptorSets;
    Freelist<DescriptorAllocatorResource> m_DescriptorAllocators;
    Freelist<PipelineLayoutResource> m_PipelineLayouts;
    Freelist<PipelineResource> m_Pipelines;
    Freelist<RenderingAttachmentResource> m_RenderingAttachments;
    Freelist<RenderingInfoResource> m_RenderingInfos;
    Freelist<FenceResource> m_Fences;
    Freelist<SemaphoreResource> m_Semaphores;
    Freelist<DependencyInfoResource> m_DependencyInfos;
    Freelist<SplitBarrierResource> m_SplitBarriers;
    Freelist<ShaderModuleResource> m_Shaders;

    std::vector<std::vector<u32>> m_CommandPoolToBuffersMap;
};

template <typename Resource>
constexpr auto DriverResources::AddResource(Resource&& resource)
{
    m_AllocatedCount++;
    
    if constexpr(std::is_same_v<std::decay_t<Resource>, DeviceResource>)
        return AddToResourceList(m_Devices, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, SwapchainResource>)
        return AddToResourceList(m_Swapchains, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, BufferResource>)
        return AddToResourceList(m_Buffers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, ImageResource>)
        return AddToResourceList(m_Images, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, SamplerResource>)
        return AddToResourceList(m_Samplers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, ViewListResource>)
        return AddToResourceList(m_ViewLists, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, CommandPoolResource>)
        return AddToResourceList(m_CommandPools, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, CommandBufferResource>)
        return AddToResourceList(m_CommandBuffers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, QueueResource>)
        return AddToResourceList(m_Queues, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, DescriptorSetLayoutResource>)
        return AddToResourceList(m_DescriptorLayouts, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, DescriptorSetResource>)
        return AddToResourceList(m_DescriptorSets, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, DescriptorAllocatorResource>)
        return AddToResourceList(m_DescriptorAllocators, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, PipelineLayoutResource>)
        return AddToResourceList(m_PipelineLayouts, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, PipelineResource>)
        return AddToResourceList(m_Pipelines, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, RenderingAttachmentResource>)
        return AddToResourceList(m_RenderingAttachments, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, RenderingInfoResource>)
        return AddToResourceList(m_RenderingInfos, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, FenceResource>)
        return AddToResourceList(m_Fences, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, SemaphoreResource>)
        return AddToResourceList(m_Semaphores, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, DependencyInfoResource>)
        return AddToResourceList(m_DependencyInfos, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, SplitBarrierResource>)
        return AddToResourceList(m_SplitBarriers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<std::decay_t<Resource>, ShaderModuleResource>)
        return AddToResourceList(m_Shaders, std::forward<Resource>(resource));
    else 
        static_assert(!sizeof(Resource), "No match for resource");
    std::unreachable();
}

template <typename ResourceList, typename Resource>
constexpr auto DriverResources::AddToResourceList(ResourceList& list, Resource&& value)
{
    static_assert(std::is_same_v<std::decay_t<Resource>, typename ResourceList::ValueType>);
    return ResourceHandle<typename ResourceList::ValueType::ObjectType>(list.Add(
        std::forward<typename ResourceList::ValueType>(value)));
}

template <typename Type>
constexpr void DriverResources::RemoveResource(const Type& type)
{
    m_DeallocatedCount++;

    if constexpr(std::is_same_v<Type, Device>)
        m_Devices.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, Swapchain>)
        m_Swapchains.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, Buffer>)
        m_Buffers.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, Image>)
        m_Images.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, Sampler>)
        m_Samplers.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, ImageViewList>)
        m_ViewLists.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, CommandPool>)
        m_CommandPools.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, CommandBuffer>)
        m_CommandBuffers.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, QueueInfo>)
        m_Queues.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, DescriptorSetLayout>)
        m_DescriptorLayouts.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, DescriptorSet>)
        m_DescriptorSets.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, DescriptorAllocator>)
        m_DescriptorAllocators.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, PipelineLayout>)
        m_PipelineLayouts.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, Pipeline>)
        m_Pipelines.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, RenderingAttachment>)
        m_RenderingAttachments.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, RenderingInfo>)
        m_RenderingInfos.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, Fence>)
        m_Fences.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, Semaphore> || std::is_same_v<Type, TimelineSemaphore>)
        m_Semaphores.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, DependencyInfo>)
        m_DependencyInfos.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, SplitBarrier>)
        m_SplitBarriers.Remove(type.Handle().m_Index);
    else if constexpr(std::is_same_v<Type, ShaderModule>)
        m_Shaders.Remove(type.Handle().m_Index);
    else 
        static_assert(!sizeof(Type), "No match for type");
}

template <typename Type>
constexpr const auto& DriverResources::operator[](const Type& type) const
{
    return const_cast<DriverResources&>(*this)[type];
}

template <typename Type>
constexpr auto& DriverResources::operator[](const Type& type)
{
    if constexpr(std::is_same_v<Type, Device>)
        return m_Devices[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, Swapchain>)
        return m_Swapchains[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, Buffer>)
        return m_Buffers[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, Image>)
        return m_Images[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, Sampler>)
        return m_Samplers[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, ImageViewList>)
        return m_ViewLists[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, CommandPool>)
        return m_CommandPools[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, CommandBuffer>)
        return m_CommandBuffers[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, QueueInfo>)
        return m_Queues[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, DescriptorSetLayout>)
        return m_DescriptorLayouts[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, DescriptorSet>)
        return m_DescriptorSets[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, DescriptorAllocator>)
        return m_DescriptorAllocators[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, PipelineLayout>)
        return m_PipelineLayouts[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, Pipeline>)
        return m_Pipelines[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, RenderingAttachment>)
        return m_RenderingAttachments[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, RenderingInfo>)
        return m_RenderingInfos[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, Fence>)
        return m_Fences[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, Semaphore> || std::is_same_v<Type, TimelineSemaphore>)
        return m_Semaphores[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, DependencyInfo>)
        return m_DependencyInfos[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, SplitBarrier>)
        return m_SplitBarriers[type.Handle().m_Index];
    else if constexpr(std::is_same_v<Type, ShaderModule>)
        return m_Shaders[type.Handle().m_Index];
    else 
        static_assert(!sizeof(Type), "No match for type");
    std::unreachable();
}

class DeletionQueue
{
public:
    ~DeletionQueue() { Flush(); }
    void AddDeleter(std::function<void()>&& deleter);
    void Flush();
private:
    std::vector<std::function<void()>> m_Deleters;
};

struct DriverState
{
    const Device* Device; 
    VmaAllocator Allocator;
    DeletionQueue DeletionQueue;
    UploadContext UploadContext;

    DriverResources Resources;
};

class Driver
{
    friend class RenderCommand;
public:
    static Device Create(const Device::Builder::CreateInfo& createInfo);
    static void Destroy(const Device& device);
    static void DeviceBuilderDefaults(Device::Builder::CreateInfo& createInfo);

    static Swapchain Create(const Swapchain::Builder::CreateInfo& createInfo);
    static void Destroy(const Swapchain& swapchain);
    static std::vector<Image> CreateSwapchainImages(const Swapchain& swapchain);
    
    static CommandBuffer Create(const CommandBuffer::Builder::CreateInfo& createInfo);
    static CommandPool Create(const CommandPool::Builder::CreateInfo& createInfo);
    static void Destroy(const CommandPool& commandPool);

    static Buffer Create(const Buffer::Builder::CreateInfo& createInfo);
    static void Destroy(const Buffer& buffer);
    static void* MapBuffer(const Buffer& buffer);
    static void UnmapBuffer(const Buffer& buffer);
    static void SetBufferData(Buffer& buffer, const void* data, u64 dataSizeBytes, u64 offsetBytes);
    static void SetBufferData(void* mappedAddress, const void* data, u64 dataSizeBytes, u64 offsetBytes);
    
    static Image AllocateImage(const Image::Builder::CreateInfo& createInfo);
    static void Destroy(const Image& image);
    static void CreateView(const ImageSubresource& image);

    static Sampler Create(const Sampler::Builder::CreateInfo& createInfo);
    static void Destroy(const Sampler& sampler);

    static ImageViewList Create(const ImageViewList::Builder::CreateInfo& createInfo);
    static void Destroy(const ImageViewList& imageViews);

    static RenderingAttachment Create(const RenderingAttachment::Builder::CreateInfo& createInfo);
    static void Destroy(const RenderingAttachment& renderingAttachment);

    static RenderingInfo Create(const RenderingInfo::Builder::CreateInfo& createInfo);
    static void Destroy(const RenderingInfo& renderingInfo);

    static ShaderModule Create(const ShaderModule::Builder::CreateInfo& createInfo);
    static void Destroy(const ShaderModule& shader);

    static PipelineLayout Create(const PipelineLayout::Builder::CreateInfo& createInfo);
    static void Destroy(const PipelineLayout& pipelineLayout);

    static Pipeline Create(const Pipeline::Builder::CreateInfo& createInfo);
    static void Destroy(const Pipeline& pipeline);
    
    static DescriptorSetLayout Create(const DescriptorSetLayout::Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorSetLayout& layout);
    
    static DescriptorSet Create(const DescriptorSet::Builder::CreateInfo& createInfo);
    static void AllocateDescriptorSet(DescriptorAllocator& allocator, DescriptorSet& set, DescriptorPoolFlags poolFlags,
        const std::vector<u32>& variableBindingCounts);
    static void DeallocateDescriptorSet(DescriptorAllocator& allocator, const DescriptorSet& set);
    static void UpdateDescriptorSet(DescriptorSet& descriptorSet, u32 slot, const Texture& texture,
        DescriptorType type, u32 arrayIndex);

    static DescriptorAllocator Create(const DescriptorAllocator::Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorAllocator& allocator);
    static void ResetAllocator(DescriptorAllocator& allocator);

    static Fence Create(const Fence::Builder::CreateInfo& createInfo);
    static void Destroy(const Fence& fence);
    
    static Semaphore Create(const Semaphore::Builder::CreateInfo& createInfo);
    static void Destroy(const Semaphore& semaphore);

    static TimelineSemaphore Create(const TimelineSemaphore::Builder::CreateInfo& createInfo);
    static void Destroy(const TimelineSemaphore& semaphore);
    static void TimelineSemaphoreWaitCPU(const TimelineSemaphore& semaphore, u64 value);
    static void TimelineSemaphoreSignalCPU(TimelineSemaphore& semaphore, u64 value);

    static SplitBarrier Create(const SplitBarrier::Builder::CreateInfo& createInfo);
    static void Destroy(const SplitBarrier& splitBarrier);

    static DependencyInfo Create(const DependencyInfo::Builder::CreateInfo& createInfo);
    static void Destroy(const DependencyInfo& dependencyInfo);
    
    template <typename Fn>
    static void ImmediateUpload(Fn&& uploadFunction);

    static void WaitIdle();
    
    static void Init(const Device& device);
    static void Shutdown();

    static const Device& GetDevice() { return *s_State.Device; }
    static DeletionQueue& DeletionQueue() { return s_State.DeletionQueue; }
    
    static u64 GetUniformBufferAlignment()
    {
        return Resources().m_Devices[0].GPUProperties.limits.minUniformBufferOffsetAlignment;
    }
    static f32 GetAnisotropyLevel()
    {
        return Resources().m_Devices[0].GPUProperties.limits.maxSamplerAnisotropy;
    }
    static u32 GetMaxIndexingImages()
    {
        return Resources().m_Devices[0].GPUDescriptorIndexingProperties.
            maxDescriptorSetUpdateAfterBindSampledImages;
    }
    static u32 GetMaxIndexingUniformBuffers()
    {
        return Resources().m_Devices[0].GPUDescriptorIndexingProperties.
            maxDescriptorSetUpdateAfterBindUniformBuffers;
    }
    static u32 GetMaxIndexingUniformBuffersDynamic()
    {
        return Resources().m_Devices[0].GPUDescriptorIndexingProperties.
            maxDescriptorSetUpdateAfterBindUniformBuffers;
    }
    static u32 GetMaxIndexingStorageBuffers()
    {
        return Resources().m_Devices[0].GPUDescriptorIndexingProperties.
            maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
    }
    static u32 GetMaxIndexingStorageBuffersDynamic()
    {
        return Resources().m_Devices[0].GPUDescriptorIndexingProperties.
            maxDescriptorSetUpdateAfterBindStorageBuffersDynamic;
    }
    static u32 GetSubgroupSize() { return Resources().m_Devices[0].GPUSubgroupProperties.subgroupSize; }
    static UploadContext* UploadContext() { return &s_State.UploadContext; }

    static Sampler GetImmutableSampler();
    
    static TracyVkCtx CreateTracyGraphicsContext(const CommandBuffer& cmd);
    static void DestroyTracyGraphicsContext(TracyVkCtx context);
    // TODO: FIX ME: direct vkapi usage
    static VkCommandBuffer GetProfilerCommandBuffer(ProfilerContext* context);
private:
    static VkDevice DeviceHandle() { return Resources().m_Devices[0].Device; }
    static VmaAllocator& Allocator() { return s_State.Allocator; }
    static void DriverCheck(VkResult res, std::string_view message)
    {
        if (res != VK_SUCCESS)
        {
            LOG(message.data());
            abort();
        }
    }
    
    static DriverResources& Resources() { return s_State.Resources; }
    static void ShutdownResources();

    static u32 GetFreePoolIndexFromAllocator(DescriptorAllocator& allocator, DescriptorPoolFlags poolFlags);

    static void CreateInstance(const Device::Builder::CreateInfo& createInfo,
        DriverResources::DeviceResource& deviceResource);
    static void CreateSurface(const Device::Builder::CreateInfo& createInfo,
        DriverResources::DeviceResource& deviceResource, Device& device);
    static void ChooseGPU(const Device::Builder::CreateInfo& createInfo,
        DriverResources::DeviceResource& deviceResource, Device& device);
    static void CreateDevice(const Device::Builder::CreateInfo& createInfo,
        DriverResources::DeviceResource& deviceResource, Device& device);
    static void RetrieveDeviceQueues(DriverResources::DeviceResource& deviceResource, Device& device);
    static void CreateDebugUtilsMessenger(DriverResources::DeviceResource& deviceResource);
    static void DestroyDebugUtilsMessenger(DriverResources::DeviceResource& deviceResource);

    struct DeviceSurfaceDetails
    {
        VkSurfaceCapabilitiesKHR Capabilities;
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR> PresentModes;
        bool IsSufficient()
        {
            return !(Formats.empty() || PresentModes.empty());
        }
    };
    static DeviceSurfaceDetails GetSurfaceDetails(const Device& device);

    static VkImageView CreateVulkanImageView(const ImageSubresource& image, VkFormat format);
    static std::pair<VkBlitImageInfo2, VkImageBlit2> CreateVulkanBlitInfo(
        const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter);
    static VkBufferImageCopy2 CreateVulkanImageCopyInfo(const ImageSubresource& subresource);

    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        const std::vector<Semaphore*>& semaphores, const std::vector<PipelineStage>& waitStages);
    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        const std::vector<TimelineSemaphore*>& semaphores,
        const std::vector<u64>& waitValues, const std::vector<PipelineStage>& waitStages);
private:
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
