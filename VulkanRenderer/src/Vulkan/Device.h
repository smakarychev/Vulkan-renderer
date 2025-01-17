#pragma once

#include "DeviceFreelist.h"
#include "Core/ProfilerContext.h"

#include "Rendering/CommandBuffer.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "Rendering/RenderingInfo.h"
#include "Rendering/Swapchain.h"
#include "Rendering/Synchronization.h"

#include "vk_mem_alloc.h"
#include <functional>

#include <volk.h>
#include <tracy/TracyVulkan.hpp>

#include "DeviceSparseSet.h"
#include "imgui/imgui.h"

class ProfilerContext;
class ShaderPipeline;

class QueueInfo
{
    FRIEND_INTERNAL
public:
    // technically any family index is possible;
    // practically GPUs have only a few
    static constexpr u32 UNSET_FAMILY = std::numeric_limits<u32>::max();
    u32 Family{UNSET_FAMILY};
private:
    ResourceHandleType<QueueInfo> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<QueueInfo> m_ResourceHandle{};
};

struct DeviceQueues
{
public:
    bool IsComplete() const
    {
        return
            Graphics.Family != QueueInfo::UNSET_FAMILY &&
            Presentation.Family != QueueInfo::UNSET_FAMILY &&
            Compute.Family != QueueInfo::UNSET_FAMILY;
    }
    std::vector<u32> AsFamilySet() const
    {
        std::vector<u32> familySet{Graphics.Family};
        if (Presentation.Family != Graphics.Family)
            familySet.push_back(Presentation.Family);
        if (Compute.Family != Graphics.Family && Compute.Family != Presentation.Family)
            familySet.push_back(Compute.Family);

        return familySet;
    }
    QueueInfo GetQueueByKind(QueueKind queueKind) const
    {
        switch (queueKind)
        {
        case QueueKind::Graphics:       return Graphics;
        case QueueKind::Presentation:   return Presentation;
        case QueueKind::Compute:        return Compute;
        default:
            ASSERT(false, "Unrecognized queue kind")
            break;
        }
        std::unreachable();
    }
    u32 GetFamilyByKind(QueueKind queueKind) const
    {
        return GetQueueByKind(queueKind).Family;
    }
public:
    QueueInfo Graphics;
    QueueInfo Presentation;
    QueueInfo Compute;
};

struct DeviceCreateInfo
{
    std::string_view AppName;
    u32 ApiVersion;
    std::vector<const char*> InstanceExtensions;
    std::vector<const char*> InstanceValidationLayers;
    std::vector<const char*> DeviceExtensions;
    GLFWwindow* Window;
    bool AsyncCompute{false};

    static DeviceCreateInfo Default(GLFWwindow* window, bool asyncCompute);
};

struct ImmediateSubmitContext
{
    CommandPool CommandPool;
    CommandBuffer CommandBuffer;
    Fence Fence;
    QueueKind QueueKind;
};

class DeviceResources
{
    FRIEND_INTERNAL

    template <typename T>
    using ResourceContainerType = DeviceSparseSet<T>;
private:
    template <typename ResourceList, typename Resource>
    constexpr auto AddToResourceList(ResourceList& list, Resource&& value);
    template <typename Resource>
    constexpr auto AddResource(Resource&& resource);
    template <typename Type>
    constexpr void RemoveResource(ResourceHandleType<Type> handle);
    template <typename Type>
    constexpr const auto& operator[](const Type& type) const;
    template <typename Type>
    constexpr auto& operator[](const Type& type);

    void MapCmdToPool(const CommandBuffer& cmd, const CommandPool& pool);
    void DestroyCmdsOfPool(ResourceHandleType<CommandPool> pool);

    void MapDescriptorSetToAllocator(const DescriptorSet& set, const DescriptorAllocator& allocator);
    void DestroyDescriptorSetsOfAllocator(ResourceHandleType<DescriptorAllocator> allocator);
    
private:
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
        struct ViewsInfo
        {
            union ViewType
            {
                u64 ViewCount;
                VkImageView View{VK_NULL_HANDLE};
            };
            ViewType ViewType;
            // in case of multiple views ViewList points to array of multiple views,
            // in case of single view it points to ViewType, as a result you can always dereference
            // ViewList and get a valid view
            VkImageView* ViewList{nullptr};
        };
        VkImage Image{VK_NULL_HANDLE};
        ViewsInfo Views{};
        VmaAllocation Allocation{VK_NULL_HANDLE};
    };
    struct SamplerResource
    {
        using ObjectType = Sampler;
        VkSampler Sampler{VK_NULL_HANDLE};
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
        using ObjectType = DescriptorsLayout;
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
    struct ShaderModuleResource
    {
        using ObjectType = ShaderModule;
        VkShaderModule Module{VK_NULL_HANDLE};
        VkShaderStageFlagBits Stage{};
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
        using ObjectType = FenceTag;
        VkFence Fence{VK_NULL_HANDLE};
    };
    struct SemaphoreResource
    {
        using ObjectType = SemaphoreTag;
        VkSemaphore Semaphore{VK_NULL_HANDLE};
    };
    struct TimelineSemaphoreResource
    {
        using ObjectType = TimelineSemaphoreTag;
        VkSemaphore Semaphore{VK_NULL_HANDLE};
        u64 Timeline{0};
    };
    struct DependencyInfoResource
    {
        using ObjectType = DependencyInfoTag;
        VkDependencyInfo DependencyInfo;
        std::vector<VkMemoryBarrier2> ExecutionMemoryDependenciesInfo;
        std::vector<VkImageMemoryBarrier2> LayoutTransitionsInfo;
    };
    struct SplitBarrierResource
    {
        using ObjectType = SplitBarrier;
        VkEvent Event{VK_NULL_HANDLE};
    };
    
    u64 m_AllocatedCount{0};
    u64 m_DeallocatedCount{0};
    
    ResourceContainerType<SwapchainResource> m_Swapchains;
    ResourceContainerType<BufferResource> m_Buffers;
    ResourceContainerType<ImageResource> m_Images;
    ResourceContainerType<SamplerResource> m_Samplers;
    ResourceContainerType<CommandPoolResource> m_CommandPools;
    ResourceContainerType<CommandBufferResource> m_CommandBuffers;
    ResourceContainerType<QueueResource> m_Queues;
    ResourceContainerType<DescriptorSetLayoutResource> m_DescriptorLayouts;
    ResourceContainerType<DescriptorSetResource> m_DescriptorSets;
    ResourceContainerType<DescriptorAllocatorResource> m_DescriptorAllocators;
    ResourceContainerType<PipelineLayoutResource> m_PipelineLayouts;
    ResourceContainerType<PipelineResource> m_Pipelines;
    ResourceContainerType<ShaderModuleResource> m_ShaderModules;
    ResourceContainerType<RenderingAttachmentResource> m_RenderingAttachments;
    ResourceContainerType<RenderingInfoResource> m_RenderingInfos;
    ResourceContainerType<FenceResource> m_Fences;
    ResourceContainerType<SemaphoreResource> m_Semaphores;
    ResourceContainerType<TimelineSemaphoreResource> m_TimelineSemaphores;
    ResourceContainerType<DependencyInfoResource> m_DependencyInfos;
    ResourceContainerType<SplitBarrierResource> m_SplitBarriers;

    std::vector<std::vector<u32>> m_CommandPoolToBuffersMap;
    std::vector<std::vector<u32>> m_DescriptorAllocatorToSetsMap;
};

template <typename ResourceList, typename Resource>
constexpr auto DeviceResources::AddToResourceList(ResourceList& list, Resource&& value)
{
    static_assert(std::is_same_v<std::decay_t<Resource>, typename ResourceList::ValueType>);
    return list.Add(std::forward<typename ResourceList::ValueType>(value));
}

template <typename Resource>
constexpr auto DeviceResources::AddResource(Resource&& resource)
{
    m_AllocatedCount++;

    using Decayed = std::decay_t<Resource>;
    
    if constexpr(std::is_same_v<Decayed, SwapchainResource>)
        return AddToResourceList(m_Swapchains, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, BufferResource>)
        return AddToResourceList(m_Buffers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, ImageResource>)
        return AddToResourceList(m_Images, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, SamplerResource>)
        return AddToResourceList(m_Samplers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, CommandPoolResource>)
        return AddToResourceList(m_CommandPools, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, CommandBufferResource>)
        return AddToResourceList(m_CommandBuffers, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, QueueResource>)
        return AddToResourceList(m_Queues, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorSetLayoutResource>)
        return AddToResourceList(m_DescriptorLayouts, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorSetResource>)
        return AddToResourceList(m_DescriptorSets, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocatorResource>)
        return AddToResourceList(m_DescriptorAllocators, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, PipelineLayoutResource>)
        return AddToResourceList(m_PipelineLayouts, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, PipelineResource>)
        return AddToResourceList(m_Pipelines, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, ShaderModuleResource>)
        return AddToResourceList(m_ShaderModules, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, RenderingAttachmentResource>)
        return AddToResourceList(m_RenderingAttachments, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, RenderingInfoResource>)
        return AddToResourceList(m_RenderingInfos, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, FenceResource>)
        return AddToResourceList(m_Fences, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, SemaphoreResource>)
        return AddToResourceList(m_Semaphores, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphoreResource>)
        return AddToResourceList(m_TimelineSemaphores, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DependencyInfoResource>)
        return AddToResourceList(m_DependencyInfos, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, SplitBarrierResource>)
        return AddToResourceList(m_SplitBarriers, std::forward<Resource>(resource));
    else 
        static_assert(!sizeof(Resource), "No match for resource");
    std::unreachable();
}

template <typename Type>
constexpr void DeviceResources::RemoveResource(ResourceHandleType<Type> handle)
{
    m_DeallocatedCount++;

    using Decayed = std::decay_t<Type>;

    if constexpr(std::is_same_v<Decayed, Swapchain>)
        m_Swapchains.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, Buffer>)
        m_Buffers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, Image>)
        m_Images.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, Sampler>)
        m_Samplers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, CommandPool>)
        m_CommandPools.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, CommandBuffer>)
        m_CommandBuffers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, QueueInfo>)
        m_Queues.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayout>)
        m_DescriptorLayouts.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorSet>)
        m_DescriptorSets.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocator>)
        m_DescriptorAllocators.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, PipelineLayout>)
        m_PipelineLayouts.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, Pipeline>)
        m_Pipelines.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, ShaderModule>)
        m_ShaderModules.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, RenderingAttachment>)
        m_RenderingAttachments.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, RenderingInfo>)
        m_RenderingInfos.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, FenceTag>)
        m_Fences.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SemaphoreTag>)
        m_Semaphores.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphoreTag>)
        m_TimelineSemaphores.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DependencyInfoTag>)
        m_DependencyInfos.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SplitBarrier>)
        m_SplitBarriers.Remove(handle);
    else 
        static_assert(!sizeof(Type), "No match for type");
}

template <typename Type>
constexpr const auto& DeviceResources::operator[](const Type& type) const
{
    return const_cast<DeviceResources&>(*this)[type];
}

template <typename Type>
constexpr auto& DeviceResources::operator[](const Type& type)
{
    using Decayed = std::decay_t<Type>;
    
    if constexpr(std::is_same_v<Decayed, Swapchain>)
        return m_Swapchains[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, Buffer>)
        return m_Buffers[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, Image>)
        return m_Images[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, Sampler>)
        return m_Samplers[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, CommandPool>)
        return m_CommandPools[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, CommandBuffer>)
        return m_CommandBuffers[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, QueueInfo>)
        return m_Queues[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayout>)
        return m_DescriptorLayouts[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, DescriptorSet>)
        return m_DescriptorSets[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocator>)
        return m_DescriptorAllocators[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, PipelineLayout>)
        return m_PipelineLayouts[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, Pipeline>)
        return m_Pipelines[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, ShaderModule>)
        return m_ShaderModules[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, RenderingAttachment>)
        return m_RenderingAttachments[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, RenderingInfo>)
        return m_RenderingInfos[type.Handle()];
    else if constexpr(std::is_same_v<Decayed, Fence>)
        return m_Fences[type];
    else if constexpr(std::is_same_v<Decayed, Semaphore>)
        return m_Semaphores[type];
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphore>)
        return m_TimelineSemaphores[type];
    else if constexpr(std::is_same_v<Decayed, DependencyInfo>)
        return m_DependencyInfos[type];
    else if constexpr(std::is_same_v<Decayed, SplitBarrier>)
        return m_SplitBarriers[type.Handle()];
    else 
        static_assert(!sizeof(Type), "No match for type");
    std::unreachable();
}

class DeletionQueue
{
    FRIEND_INTERNAL
public:
    ~DeletionQueue() { Flush(); }

    template <typename Type>
    void Enqueue(Type& type);

    void Flush();
private:
    bool m_IsDummy{false};
    std::vector<ResourceHandleType<Swapchain>> m_Swapchains;
    std::vector<ResourceHandleType<Buffer>> m_Buffers;
    std::vector<ResourceHandleType<Image>> m_Images;
    std::vector<ResourceHandleType<Sampler>> m_Samplers;
    std::vector<ResourceHandleType<CommandPool>> m_CommandPools;
    std::vector<ResourceHandleType<QueueInfo>> m_Queues;
    std::vector<ResourceHandleType<DescriptorsLayout>> m_DescriptorLayouts;
    std::vector<ResourceHandleType<DescriptorAllocator>> m_DescriptorAllocators;
    std::vector<ResourceHandleType<PipelineLayout>> m_PipelineLayouts;
    std::vector<ResourceHandleType<Pipeline>> m_Pipelines;
    std::vector<ResourceHandleType<ShaderModule>> m_ShaderModules;
    std::vector<ResourceHandleType<RenderingAttachment>> m_RenderingAttachments;
    std::vector<ResourceHandleType<RenderingInfo>> m_RenderingInfos;
    std::vector<Fence> m_Fences;
    std::vector<Semaphore> m_Semaphores;
    std::vector<TimelineSemaphore> m_TimelineSemaphore;
    std::vector<DependencyInfo> m_DependencyInfos;
    std::vector<ResourceHandleType<SplitBarrier>> m_SplitBarriers;
};


template <typename Type>
void DeletionQueue::Enqueue(Type& type)
{
    using Decayed = std::decay_t<Type>;
    
    if (m_IsDummy)
        return;
    
    if constexpr(std::is_same_v<Decayed, Swapchain>)
        m_Swapchains.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, Buffer>)
        m_Buffers.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, Image>)
        m_Images.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, Sampler>)
        m_Samplers.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, CommandPool>)
        m_CommandPools.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, QueueInfo>)
        m_Queues.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayout>)
        m_DescriptorLayouts.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocator>)
        m_DescriptorAllocators.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, PipelineLayout>)
        m_PipelineLayouts.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, Pipeline>)
        m_Pipelines.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, ShaderPipeline>)
        m_Pipelines.push_back(type.m_Pipeline.Handle());
    else if constexpr(std::is_same_v<Decayed, ShaderModule>)
        m_ShaderModules.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, RenderingAttachment>)
        m_RenderingAttachments.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, RenderingInfo>)
        m_RenderingInfos.push_back(type.Handle());
    else if constexpr(std::is_same_v<Decayed, Fence>)
        m_Fences.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Semaphore>)
        m_Semaphores.push_back(type);
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphore>)
        m_TimelineSemaphore.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DependencyInfo>)
        m_DependencyInfos.push_back(type);
    else if constexpr(std::is_same_v<Decayed, SplitBarrier>)
        m_SplitBarriers.push_back(type.Handle());
    else 
        static_assert(!sizeof(Type), "No match for type");
}

class Device
{
    friend class RenderCommand;
public:
    static void Destroy(ResourceHandleType<QueueInfo> queue);

    static Swapchain CreateSwapchain(SwapchainCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<Swapchain> swapchain);
    static std::vector<Image> CreateSwapchainImages(const Swapchain& swapchain);
    static void DestroySwapchainImages(const Swapchain& swapchain);
    static u32 AcquireNextImage(const Swapchain& swapchain, const SwapchainFrameSync& swapchainFrameSync);
    static bool Present(const Swapchain& swapchain, QueueKind queueKind, const SwapchainFrameSync& swapchainFrameSync,
        u32 imageIndex);
    
    static CommandBuffer CreateCommandBuffer(CommandBufferCreateInfo&& createInfo);
    static CommandPool CreateCommandPool(CommandPoolCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<CommandPool> commandPool);
    static void ResetPool(const CommandPool& pool);
    static void ResetCommandBuffer(const CommandBuffer& cmd);
    static void BeginCommandBuffer(const CommandBuffer& cmd, CommandBufferUsage usage);
    static void EndCommandBuffer(const CommandBuffer& cmd);
    static void SubmitCommandBuffer(const CommandBuffer& cmd, QueueKind queueKind,
        const BufferSubmitSyncInfo& submitSync);
    static void SubmitCommandBuffer(const CommandBuffer& cmd, QueueKind queueKind,
        const BufferSubmitTimelineSyncInfo& submitSync);
    static void SubmitCommandBuffer(const CommandBuffer& cmd, QueueKind queueKind, Fence fence);
    static void SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, QueueKind queueKind,
        const BufferSubmitSyncInfo& submitSync);
    static void SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, QueueKind queueKind,
        const BufferSubmitTimelineSyncInfo& submitSync);

    static Buffer CreateBuffer(BufferCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<Buffer> buffer);
    static Buffer CreateStagingBuffer(u64 sizeBytes);
    static void* MapBuffer(const Buffer& buffer);
    static void UnmapBuffer(const Buffer& buffer);
    static void SetBufferData(Buffer& buffer, Span<const std::byte> data, u64 offsetBytes);
    static void SetBufferData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes);
    static u64 GetDeviceAddress(const Buffer& buffer);
    
    static Image CreateImage(ImageCreateInfo&& createInfo, DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(ResourceHandleType<Image> image);
    static void CreateViews(const ImageSubresource& image,
        const std::vector<ImageSubresourceDescription>& additionalViews);
    static void CalculateMipmaps(const Image& image, const CommandBuffer& cmd, ImageLayout currentLayout);

    static Sampler CreateSampler(SamplerCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<Sampler> sampler);

    static RenderingAttachment CreateRenderingAttachment(RenderingAttachmentCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<RenderingAttachment> renderingAttachment);

    static RenderingInfo CreateRenderingInfo(RenderingInfoCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<RenderingInfo> renderingInfo);

    static PipelineLayout CreatePipelineLayout(PipelineLayoutCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<PipelineLayout> pipelineLayout);

    static Pipeline CreatePipeline(PipelineCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<Pipeline> pipeline);

    static ShaderModule CreateShaderModule(ShaderModuleCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<ShaderModule> shaderModule);
    
    static DescriptorsLayout CreateDescriptorsLayout(DescriptorsLayoutCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<DescriptorsLayout> layout);
    
    static DescriptorSet CreateDescriptorSet(DescriptorSetCreateInfo&& createInfo);
    static void AllocateDescriptorSet(DescriptorAllocator& allocator, DescriptorSet& set, DescriptorPoolFlags poolFlags,
        const std::vector<u32>& variableBindingCounts);
    static void DeallocateDescriptorSet(ResourceHandleType<DescriptorAllocator> allocator,
        ResourceHandleType<DescriptorSet> set);
    static void UpdateDescriptorSet(DescriptorSet& descriptorSet, u32 slot, const Texture& texture,
        DescriptorType type, u32 arrayIndex);

    static DescriptorAllocator CreateDescriptorAllocator(DescriptorAllocatorCreateInfo&& createInfo);
    static void Destroy(ResourceHandleType<DescriptorAllocator> allocator);
    static void ResetAllocator(DescriptorAllocator& allocator);

    static DescriptorArenaAllocator CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo);
    static std::optional<Descriptors> Allocate(DescriptorArenaAllocator& allocator,
        DescriptorsLayout layout, const DescriptorAllocatorAllocationBindings& bindings);
        
    static void UpdateDescriptors(const Descriptors& descriptors, u32 slot, const BufferBindingInfo& buffer,
        DescriptorType type, u32 index);  
    static void UpdateDescriptors(const Descriptors& descriptors, u32 slot, const TextureBindingInfo& texture,
        DescriptorType type, u32 index);  
    static void UpdateGlobalDescriptors(const Descriptors& descriptors, u32 slot, const BufferBindingInfo& buffer,
        DescriptorType type, u32 index);  
    static void UpdateGlobalDescriptors(const Descriptors& descriptors, u32 slot, const TextureBindingInfo& texture,
        DescriptorType type, u32 index);

    static Fence CreateFence(FenceCreateInfo&& createInfo, DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Fence fence);
    static void WaitForFence(const Fence& fence);
    static bool CheckFence(const Fence& fence);
    static void ResetFence(const Fence& fence);
    
    static Semaphore CreateSemaphore(DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Semaphore semaphore);

    static TimelineSemaphore CreateTimelineSemaphore(TimelineSemaphoreCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(TimelineSemaphore semaphore);
    static void TimelineSemaphoreWaitCPU(const TimelineSemaphore& semaphore, u64 value);
    static void TimelineSemaphoreSignalCPU(TimelineSemaphore& semaphore, u64 value);

    static DependencyInfo CreateDependencyInfo(DependencyInfoCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(DependencyInfo dependencyInfo);

    static SplitBarrier CreateSplitBarrier();
    static void Destroy(ResourceHandleType<SplitBarrier> splitBarrier);
    
    template <typename Fn>
    static void ImmediateSubmit(Fn&& uploadFunction);

    static void WaitIdle();
    
    static void Init(DeviceCreateInfo&& createInfo);
    static void Shutdown();

    static DeletionQueue& DeletionQueue();
    static ::DeletionQueue& DummyDeletionQueue();

    static u64 GetUniformBufferAlignment();

    static f32 GetAnisotropyLevel();

    static u32 GetMaxIndexingImages();

    static u32 GetMaxIndexingUniformBuffers();

    static u32 GetMaxIndexingUniformBuffersDynamic();

    static u32 GetMaxIndexingStorageBuffers();

    static u32 GetMaxIndexingStorageBuffersDynamic();
    static u32 GetSubgroupSize();
    static ImmediateSubmitContext* SubmitContext();

    static TracyVkCtx CreateTracyGraphicsContext(const CommandBuffer& cmd);
    static void DestroyTracyGraphicsContext(TracyVkCtx context);
    // TODO: FIX ME: direct vkapi usage
    static VkCommandBuffer GetProfilerCommandBuffer(ProfilerContext* context);

    static ImTextureID CreateImGuiImage(const ImageSubresource& texture, Sampler sampler, ImageLayout layout);
    static void DestroyImGuiImage(ImTextureID image);

    static void DumpMemoryStats(const std::filesystem::path& path);
private:
    static VmaAllocator& Allocator();

    static void DeviceCheck(VkResult res, std::string_view message)
    {
        if (res != VK_SUCCESS)
        {
            LOG(message.data());
            abort();
        }
    }
    
    static DeviceResources& Resources();
    static void ShutdownResources();

    static void InitImGuiUI();
    static void ShutdownImGuiUI();

    static u32 GetFreePoolIndexFromAllocator(DescriptorAllocator& allocator, DescriptorPoolFlags poolFlags);

    static void CreateInstance(const DeviceCreateInfo& createInfo);
    static void CreateSurface(const DeviceCreateInfo& createInfo);
    static void ChooseGPU(const DeviceCreateInfo& createInfo);
    static void CreateDevice(const DeviceCreateInfo& createInfo);
    static void RetrieveDeviceQueues();
    static void CreateDebugUtilsMessenger();
    static void DestroyDebugUtilsMessenger();

    static u32 GetDescriptorSizeBytes(DescriptorType type);

    static DeviceResources::BufferResource CreateBufferResource(u64 sizeBytes, VkBufferUsageFlags usage,
        VmaAllocationCreateFlags allocationFlags);

    static Image CreateImageFromAssetFile(ImageCreateInfo& createInfo, ImageAssetPath assetPath);
    static Image CreateImageFromPixels(ImageCreateInfo& createInfo, Span<const std::byte> pixels);
    static Image CreateImageFromBuffer(ImageCreateInfo& createInfo, Buffer buffer);
    static void PreprocessCreateInfo(ImageCreateInfo& createInfo);
    static DeviceResources::ImageResource CreateImageResource(ImageCreateInfo& createInfo);
    static VkImageView CreateVulkanImageView(const ImageSubresource& image, VkFormat format);
    static std::pair<VkBlitImageInfo2, VkImageBlit2> CreateVulkanBlitInfo(
        const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter);
    static std::pair<VkCopyImageInfo2, VkImageCopy2> CreateVulkanImageCopyInfo(
        const ImageCopyInfo& source, const ImageCopyInfo& destination);
    static VkBufferImageCopy2 CreateVulkanImageCopyInfo(const ImageSubresource& subresource);

    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        const std::vector<Semaphore*>& semaphores, const std::vector<PipelineStage>& waitStages);
    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        const std::vector<TimelineSemaphore*>& semaphores,
        const std::vector<u64>& waitValues, const std::vector<PipelineStage>& waitStages);
private:
    struct State;
    static State s_State;
};

template <typename Fn>
void Device::ImmediateSubmit(Fn&& uploadFunction)
{
    auto&& [pool, cmd, fence, queue] = *SubmitContext();
    
    cmd.Begin();

    
    uploadFunction(cmd);

    
    cmd.End();
    cmd.Submit(queue, fence);
    WaitForFence(fence);
    ResetFence(fence);
    pool.Reset();
}
