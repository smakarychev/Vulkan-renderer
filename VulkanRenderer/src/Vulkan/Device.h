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

    void MapCmdToPool(CommandBuffer cmd, CommandPool pool);
    void DestroyCmdsOfPool(CommandPool pool);

    void MapDescriptorSetToAllocator(DescriptorSet set, DescriptorAllocator allocator);
    void DestroyDescriptorSetsOfAllocator(DescriptorAllocator allocator);
    
private:
    struct SwapchainResource
    {
        using ObjectType = SwapchainTag;
        VkSwapchainKHR Swapchain{VK_NULL_HANDLE};
        VkFormat ColorFormat{};
        SwapchainDescription Description{};
    };
    struct BufferResource
    {
        using ObjectType = BufferTag;
        VkBuffer Buffer{VK_NULL_HANDLE};
        BufferDescription Description{};
        void* HostAddress{nullptr};
        VmaAllocation Allocation{VK_NULL_HANDLE};
    };
    struct ImageResource
    {
        using ObjectType = ImageTag;
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
        ImageDescription Description{};
        VmaAllocation Allocation{VK_NULL_HANDLE};
    };
    struct SamplerResource
    {
        using ObjectType = SamplerTag;
        VkSampler Sampler{VK_NULL_HANDLE};
    };
    struct CommandPoolResource
    {
        using ObjectType = CommandPoolTag;
        VkCommandPool CommandPool{VK_NULL_HANDLE};
    };
    struct CommandBufferResource
    {
        using ObjectType = CommandBufferTag;
        VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
        CommandBufferKind Kind{CommandBufferKind::Primary};
    };
    struct DescriptorSetLayoutResource
    {
        using ObjectType = DescriptorsLayoutTag;
        VkDescriptorSetLayout Layout{VK_NULL_HANDLE};
    };
    struct DescriptorSetResource
    {
        using ObjectType = DescriptorSetTag;
        VkDescriptorSet DescriptorSet{VK_NULL_HANDLE};
        VkDescriptorPool Pool{VK_NULL_HANDLE};
        DescriptorAllocator Allocator{};
        DescriptorsLayout Layout{};
    };
    struct DescriptorAllocatorResource
    {
        using ObjectType = DescriptorAllocatorTag;
        struct PoolInfo
        {
            VkDescriptorPool Pool;
            DescriptorPoolFlags Flags;
            u32 AllocationCount{0};
        };
        struct PoolSize
        {
            DescriptorType DescriptorType;
            f32 SetSizeMultiplier;
        };
        std::vector<PoolInfo> FreePools;
        std::vector<PoolInfo> UsedPools;
        std::vector<PoolSize> PoolSizes = {
            {DescriptorType::Sampler, 0.5f},
            {DescriptorType::Image, 4.0f},
            {DescriptorType::ImageStorage, 1.0f},
            {DescriptorType::TexelUniform, 1.0f},
            {DescriptorType::TexelStorage, 1.0f},
            {DescriptorType::UniformBuffer, 2.0f},
            {DescriptorType::StorageBuffer, 2.0f},
            {DescriptorType::UniformBufferDynamic, 1.0f},
            {DescriptorType::StorageBufferDynamic, 1.0f},
            {DescriptorType::Input, 0.5f}
        };
        u32 MaxSetsPerPool{};
    };
    struct DescriptorsResource
    {
        using ObjectType = DescriptorsTag;
        std::vector<u64> Offsets{};
        u64 SizeBytes{0};
        DescriptorArenaAllocator Allocator{};
    };
    struct DescriptorArenaAllocatorResource
    {
        using ObjectType = DescriptorArenaAllocatorTag;
        std::array<void*, BUFFERED_FRAMES> MappedAddresses;
        u64 SizeBytes{0};
        u32 CurrentBuffer{0};
        u64 CurrentOffset{0};
        DescriptorsKind Kind{DescriptorsKind::Resource};
        DescriptorAllocatorResidence Residence{DescriptorAllocatorResidence::CPU};
        std::array<VkBuffer, BUFFERED_FRAMES> Buffers;
        std::array<VmaAllocation, BUFFERED_FRAMES> Allocations;
    };
    struct PipelineLayoutResource
    {
        using ObjectType = PipelineLayoutTag;
        VkPipelineLayout Layout{VK_NULL_HANDLE};
        std::vector<VkPushConstantRange> PushConstants;
    };
    struct PipelineResource
    {
        using ObjectType = PipelineTag;
        VkPipeline Pipeline{VK_NULL_HANDLE};
    };
    struct ShaderModuleResource
    {
        using ObjectType = ShaderModuleTag;
        VkShaderModule Module{VK_NULL_HANDLE};
        VkShaderStageFlagBits Stage{};
    };
    struct RenderingAttachmentResource
    {
        using ObjectType = RenderingAttachmentTag;
        VkRenderingAttachmentInfo AttachmentInfo{};
    };
    struct RenderingInfoResource
    {
        using ObjectType = RenderingInfoTag;
        std::vector<VkRenderingAttachmentInfo> ColorAttachments{};
        std::optional<VkRenderingAttachmentInfo> DepthAttachment{};
        glm::uvec2 RenderArea{};
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
        using ObjectType = SplitBarrierTag;
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
    ResourceContainerType<DescriptorSetLayoutResource> m_DescriptorLayouts;
    ResourceContainerType<DescriptorSetResource> m_DescriptorSets;
    ResourceContainerType<DescriptorAllocatorResource> m_DescriptorAllocators;
    ResourceContainerType<DescriptorsResource> m_Descriptors;
    ResourceContainerType<DescriptorArenaAllocatorResource> m_DescriptorArenaAllocators;
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
    else if constexpr(std::is_same_v<Decayed, DescriptorSetLayoutResource>)
        return AddToResourceList(m_DescriptorLayouts, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorSetResource>)
        return AddToResourceList(m_DescriptorSets, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocatorResource>)
        return AddToResourceList(m_DescriptorAllocators, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorsResource>)
        return AddToResourceList(m_Descriptors, std::forward<Resource>(resource));
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocatorResource>)
        return AddToResourceList(m_DescriptorArenaAllocators, std::forward<Resource>(resource));
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

    if constexpr(std::is_same_v<Decayed, SwapchainTag>)
        m_Swapchains.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, BufferTag>)
        m_Buffers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, ImageTag>)
        m_Images.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SamplerTag>)
        m_Samplers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, CommandPoolTag>)
        m_CommandPools.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, CommandBufferTag>)
        m_CommandBuffers.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayoutTag>)
        m_DescriptorLayouts.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorSetTag>)
        m_DescriptorSets.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocatorTag>)
        m_DescriptorAllocators.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorsTag>)
        m_Descriptors.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocatorTag>)
        m_DescriptorArenaAllocators.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, PipelineLayoutTag>)
        m_PipelineLayouts.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, PipelineTag>)
        m_Pipelines.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, ShaderModuleTag>)
        m_ShaderModules.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, RenderingAttachmentTag>)
        m_RenderingAttachments.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, RenderingInfoTag>)
        m_RenderingInfos.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, FenceTag>)
        m_Fences.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SemaphoreTag>)
        m_Semaphores.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphoreTag>)
        m_TimelineSemaphores.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, DependencyInfoTag>)
        m_DependencyInfos.Remove(handle);
    else if constexpr(std::is_same_v<Decayed, SplitBarrierTag>)
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
        return m_Swapchains[type];
    else if constexpr(std::is_same_v<Decayed, Buffer>)
        return m_Buffers[type];
    else if constexpr(std::is_same_v<Decayed, Image>)
        return m_Images[type];
    else if constexpr(std::is_same_v<Decayed, Sampler>)
        return m_Samplers[type];
    else if constexpr(std::is_same_v<Decayed, CommandPool>)
        return m_CommandPools[type];
    else if constexpr(std::is_same_v<Decayed, CommandBuffer>)
        return m_CommandBuffers[type];
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayout>)
        return m_DescriptorLayouts[type];
    else if constexpr(std::is_same_v<Decayed, DescriptorSet>)
        return m_DescriptorSets[type];
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocator>)
        return m_DescriptorAllocators[type];
    else if constexpr(std::is_same_v<Decayed, Descriptors>)
        return m_Descriptors[type];
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocator>)
        return m_DescriptorArenaAllocators[type];
    else if constexpr(std::is_same_v<Decayed, PipelineLayout>)
        return m_PipelineLayouts[type];
    else if constexpr(std::is_same_v<Decayed, Pipeline>)
        return m_Pipelines[type];
    else if constexpr(std::is_same_v<Decayed, ShaderModule>)
        return m_ShaderModules[type];
    else if constexpr(std::is_same_v<Decayed, RenderingAttachment>)
        return m_RenderingAttachments[type];
    else if constexpr(std::is_same_v<Decayed, RenderingInfo>)
        return m_RenderingInfos[type];
    else if constexpr(std::is_same_v<Decayed, Fence>)
        return m_Fences[type];
    else if constexpr(std::is_same_v<Decayed, Semaphore>)
        return m_Semaphores[type];
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphore>)
        return m_TimelineSemaphores[type];
    else if constexpr(std::is_same_v<Decayed, DependencyInfo>)
        return m_DependencyInfos[type];
    else if constexpr(std::is_same_v<Decayed, SplitBarrier>)
        return m_SplitBarriers[type];
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
    std::vector<Swapchain> m_Swapchains;
    std::vector<Buffer> m_Buffers;
    std::vector<Image> m_Images;
    std::vector<Sampler> m_Samplers;
    std::vector<CommandPool> m_CommandPools;
    std::vector<DescriptorsLayout> m_DescriptorLayouts;
    std::vector<DescriptorAllocator> m_DescriptorAllocators;
    std::vector<DescriptorArenaAllocator> m_DescriptorArenaAllocators;
    std::vector<PipelineLayout> m_PipelineLayouts;
    std::vector<Pipeline> m_Pipelines;
    std::vector<ShaderModule> m_ShaderModules;
    std::vector<RenderingAttachment> m_RenderingAttachments;
    std::vector<RenderingInfo> m_RenderingInfos;
    std::vector<Fence> m_Fences;
    std::vector<Semaphore> m_Semaphores;
    std::vector<TimelineSemaphore> m_TimelineSemaphore;
    std::vector<DependencyInfo> m_DependencyInfos;
    std::vector<SplitBarrier> m_SplitBarriers;
};


template <typename Type>
void DeletionQueue::Enqueue(Type& type)
{
    using Decayed = std::decay_t<Type>;
    
    if (m_IsDummy)
        return;
    
    if constexpr(std::is_same_v<Decayed, Swapchain>)
        m_Swapchains.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Buffer>)
        m_Buffers.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Image>)
        m_Images.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Sampler>)
        m_Samplers.push_back(type);
    else if constexpr(std::is_same_v<Decayed, CommandPool>)
        m_CommandPools.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DescriptorsLayout>)
        m_DescriptorLayouts.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DescriptorAllocator>)
        m_DescriptorAllocators.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DescriptorArenaAllocator>)
        m_DescriptorArenaAllocators.push_back(type);
    else if constexpr(std::is_same_v<Decayed, PipelineLayout>)
        m_PipelineLayouts.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Pipeline>)
        m_Pipelines.push_back(type);
    else if constexpr(std::is_same_v<Decayed, ShaderPipeline>)
        m_Pipelines.push_back(type.m_Pipeline.Handle());
    else if constexpr(std::is_same_v<Decayed, ShaderModule>)
        m_ShaderModules.push_back(type);
    else if constexpr(std::is_same_v<Decayed, RenderingAttachment>)
        m_RenderingAttachments.push_back(type);
    else if constexpr(std::is_same_v<Decayed, RenderingInfo>)
        m_RenderingInfos.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Fence>)
        m_Fences.push_back(type);
    else if constexpr(std::is_same_v<Decayed, Semaphore>)
        m_Semaphores.push_back(type);
    else if constexpr(std::is_same_v<Decayed, TimelineSemaphore>)
        m_TimelineSemaphore.push_back(type);
    else if constexpr(std::is_same_v<Decayed, DependencyInfo>)
        m_DependencyInfos.push_back(type);
    else if constexpr(std::is_same_v<Decayed, SplitBarrier>)
        m_SplitBarriers.push_back(type);
    else 
        static_assert(!sizeof(Type), "No match for type");
}

class Device
{
    friend class RenderCommand;
public:
    static Swapchain CreateSwapchain(SwapchainCreateInfo&& createInfo, DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Swapchain swapchain);
    static u32 AcquireNextImage(Swapchain swapchain, u32 frameNumber);
    static bool Present(Swapchain swapchain, QueueKind queueKind, u32 frameNumber, u32 imageIndex);
    static SwapchainDescription& GetSwapchainDescription(Swapchain swapchain);
    
    static CommandBuffer CreateCommandBuffer(CommandBufferCreateInfo&& createInfo);
    static CommandPool CreateCommandPool(CommandPoolCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(CommandPool commandPool);
    static void ResetPool(CommandPool pool);
    static void ResetCommandBuffer(CommandBuffer cmd);
    static void BeginCommandBuffer(CommandBuffer cmd);
    static void BeginCommandBuffer(CommandBuffer cmd, CommandBufferUsage usage);
    static void EndCommandBuffer(CommandBuffer cmd);
    static void SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind,
        const BufferSubmitSyncInfo& submitSync);
    static void SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind,
        const BufferSubmitTimelineSyncInfo& submitSync);
    static void SubmitCommandBuffer(CommandBuffer cmd, QueueKind queueKind, Fence fence);
    static void SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
        const BufferSubmitSyncInfo& submitSync);
    static void SubmitCommandBuffers(Span<const CommandBuffer> cmds, QueueKind queueKind,
        const BufferSubmitTimelineSyncInfo& submitSync);

    static Buffer CreateBuffer(BufferCreateInfo&& createInfo, DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Buffer buffer);
    static Buffer CreateStagingBuffer(u64 sizeBytes);
    static void* MapBuffer(Buffer buffer);
    static void UnmapBuffer(Buffer buffer);
    static void SetBufferData(Buffer buffer, Span<const std::byte> data, u64 offsetBytes);
    static void SetBufferData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes);
    static void* GetBufferMappedAddress(Buffer buffer);
    static usize GetBufferSizeBytes(Buffer buffer);
    static const BufferDescription& GetBufferDescription(Buffer buffer);
    static u64 GetDeviceAddress(Buffer buffer);
    
    static Image CreateImage(ImageCreateInfo&& createInfo, DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Image image);
    static void CreateViews(const ImageSubresource& image,
        const std::vector<ImageSubresourceDescription>& additionalViews);
    static void CalculateMipmaps(Image image, CommandBuffer cmd, ImageLayout currentLayout);
    static Span<const ImageSubresourceDescription> GetAdditionalImageViews(Image image);
    static ImageViewHandle GetImageViewHandle(Image image, ImageSubresourceDescription subresourceDescription);
    static const ImageDescription& GetImageDescription(Image image);
    
    static Sampler CreateSampler(SamplerCreateInfo&& createInfo);
    static void Destroy(Sampler sampler);

    static RenderingAttachment CreateRenderingAttachment(RenderingAttachmentCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(RenderingAttachment renderingAttachment);

    static RenderingInfo CreateRenderingInfo(RenderingInfoCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(RenderingInfo renderingInfo);

    static PipelineLayout CreatePipelineLayout(PipelineLayoutCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(PipelineLayout pipelineLayout);

    static Pipeline CreatePipeline(PipelineCreateInfo&& createInfo, DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Pipeline pipeline);

    static ShaderModule CreateShaderModule(ShaderModuleCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(ShaderModule shaderModule);
    
    static DescriptorsLayout CreateDescriptorsLayout(DescriptorsLayoutCreateInfo&& createInfo);
    static void Destroy(DescriptorsLayout layout);
    
    static DescriptorSet CreateDescriptorSet(DescriptorSetCreateInfo&& createInfo);
    static DescriptorSet AllocateDescriptorSet(DescriptorAllocator allocator, DescriptorsLayout layout,
        DescriptorPoolFlags poolFlags, const std::vector<u32>& variableBindingCounts);
    static void DeallocateDescriptorSet(DescriptorAllocator allocator,  DescriptorSet set);
    static void UpdateDescriptorSet(DescriptorSet descriptorSet, DescriptorBindingInfo bindingInfo,
        const ImageSubresource& image, Sampler sampler, ImageLayout layout, u32 index);

    static DescriptorAllocator CreateDescriptorAllocator(DescriptorAllocatorCreateInfo&& createInfo);
    static void Destroy(DescriptorAllocator allocator);
    static void ResetAllocator(DescriptorAllocator allocator);

    static DescriptorArenaAllocator CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(DescriptorArenaAllocator allocator);
    static std::optional<Descriptors> AllocateDescriptors(DescriptorArenaAllocator allocator,
        DescriptorsLayout layout, const DescriptorAllocatorAllocationBindings& bindings);
    static void ResetDescriptorArenaAllocator(DescriptorArenaAllocator allocator);
    static DescriptorsKind GetDescriptorArenaAllocatorKind(DescriptorArenaAllocator allocator);
    
    static void UpdateDescriptors(Descriptors descriptors, DescriptorBindingInfo bindingInfo,
        const BufferSubresource& buffer, u32 index);  
    static void UpdateDescriptors(Descriptors descriptors, DescriptorBindingInfo bindingInfo, Sampler sampler);  
    static void UpdateDescriptors(Descriptors descriptors, DescriptorBindingInfo bindingInfo,
        const ImageSubresource& image, ImageLayout layout, u32 index);  
    static void UpdateGlobalDescriptors(Descriptors descriptors, DescriptorBindingInfo bindingInfo,
        const BufferSubresource& buffer, u32 index);
    static void UpdateGlobalDescriptors(Descriptors descriptors, DescriptorBindingInfo bindingInfo, Sampler sampler);
    static void UpdateGlobalDescriptors(Descriptors descriptors, DescriptorBindingInfo bindingInfo,
        const ImageSubresource& image, ImageLayout layout, u32 index);

    static Fence CreateFence(FenceCreateInfo&& createInfo, DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Fence fence);
    static void WaitForFence(Fence fence);
    static bool CheckFence(Fence fence);
    static void ResetFence(Fence fence);
    
    static Semaphore CreateSemaphore(DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(Semaphore semaphore);

    static TimelineSemaphore CreateTimelineSemaphore(TimelineSemaphoreCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(TimelineSemaphore semaphore);
    static void TimelineSemaphoreWaitCPU(TimelineSemaphore semaphore, u64 value);
    static void TimelineSemaphoreSignalCPU(TimelineSemaphore semaphore, u64 value);

    static DependencyInfo CreateDependencyInfo(DependencyInfoCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(DependencyInfo dependencyInfo);

    static SplitBarrier CreateSplitBarrier(DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(SplitBarrier splitBarrier);
    
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

    static TracyVkCtx CreateTracyGraphicsContext(CommandBuffer cmd);
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

    static u32 GetFreePoolIndexFromAllocator(DescriptorAllocator allocator, DescriptorPoolFlags poolFlags);

    static void CreateInstance(const DeviceCreateInfo& createInfo);
    static void CreateSurface(const DeviceCreateInfo& createInfo);
    static void ChooseGPU(const DeviceCreateInfo& createInfo);
    static void CreateDevice(const DeviceCreateInfo& createInfo);
    static void RetrieveDeviceQueues();
    static void CreateDebugUtilsMessenger();
    static void DestroyDebugUtilsMessenger();

    static void CreateSwapchainImages(Swapchain swapchain);
    static void DestroySwapchainImages(Swapchain swapchain);

    static u32 GetDescriptorSizeBytes(DescriptorType type);
    static void WriteDescriptor(Descriptors descriptors, DescriptorBindingInfo bindingInfo, u32 index,
        VkDescriptorGetInfoEXT& descriptorGetInfo);
    
    static DeviceResources::BufferResource CreateBufferResource(u64 sizeBytes, VkBufferUsageFlags usage,
        VmaAllocationCreateFlags allocationFlags);
    static u64 GetDeviceAddress(VkBuffer buffer);

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
        Span<const Semaphore> semaphores, Span<const PipelineStage> waitStages);
    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        Span<const TimelineSemaphore> semaphores,
        Span<const u64> waitValues, Span<const PipelineStage> waitStages);
private:
    struct State;
    static State s_State;
};

template <typename Fn>
void Device::ImmediateSubmit(Fn&& uploadFunction)
{
    auto&& [pool, cmd, fence, queue] = *SubmitContext();

    BeginCommandBuffer(cmd);

    
    uploadFunction(cmd);


    EndCommandBuffer(cmd);
    SubmitCommandBuffer(cmd, queue, fence);
    WaitForFence(fence);
    ResetFence(fence);
    ResetPool(pool);
}
