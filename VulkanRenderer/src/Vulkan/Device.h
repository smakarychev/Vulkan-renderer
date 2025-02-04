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
#include "Rendering/Commands/RenderCommandList.h"

struct FrameContext;
class DeviceResources;
struct CopyBufferCommand;
struct CopyBufferToImageCommand;
class ProfilerContext;

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
    RenderCommandList CommandList;
    Fence Fence;
    QueueKind QueueKind;
};

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
public:
    static void BeginFrame(FrameContext& ctx);
    
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
    static void ResizeBuffer(Buffer buffer, u64 newSize, RenderCommandList& cmdList, bool copyData = true);
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
    static void CreateMipmaps(Image image, RenderCommandList& cmdList,
        ImageLayout currentLayout);
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

    static void CompileCommand(CommandBuffer cmd, const ExecuteSecondaryBufferCommand& command);
    
    static void CompileCommand(CommandBuffer cmd, const PrepareSwapchainPresentCommand& command);

    static void CompileCommand(CommandBuffer cmd, const BeginRenderingCommand& command);
    static void CompileCommand(CommandBuffer cmd, const EndRenderingCommand& command);
    
    static void CompileCommand(CommandBuffer cmd, const ImGuiBeginCommand& command);
    static void CompileCommand(CommandBuffer cmd, const ImGuiEndCommand& command);

    static void CompileCommand(CommandBuffer cmd, const BeginConditionalRenderingCommand& command);
    static void CompileCommand(CommandBuffer cmd, const EndConditionalRenderingCommand& command);
    
    static void CompileCommand(CommandBuffer cmd, const SetViewportCommand& command);
    static void CompileCommand(CommandBuffer cmd, const SetScissorsCommand& command);
    static void CompileCommand(CommandBuffer cmd, const SetDepthBiasCommand& command);
    
    static void CompileCommand(CommandBuffer cmd, const CopyBufferCommand& command);
    static void CompileCommand(CommandBuffer cmd, const CopyBufferToImageCommand& command);

    static void CompileCommand(CommandBuffer cmd, const CopyImageCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BlitImageCommand& command);

    static void CompileCommand(CommandBuffer cmd, const WaitOnFullPipelineBarrierCommand& command);
    static void CompileCommand(CommandBuffer cmd, const WaitOnBarrierCommand& command);
    static void CompileCommand(CommandBuffer cmd, const SignalSplitBarrierCommand& command);
    static void CompileCommand(CommandBuffer cmd, const WaitOnSplitBarrierCommand& command);
    static void CompileCommand(CommandBuffer cmd, const ResetSplitBarrierCommand& command);

    static void CompileCommand(CommandBuffer cmd, const BindVertexBuffersCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindIndexU32BufferCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindIndexU16BufferCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindIndexU8BufferCommand& command);

    static void CompileCommand(CommandBuffer cmd, const BindPipelineGraphicsCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindPipelineComputeCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindImmutableSamplersGraphicsCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindImmutableSamplersComputeCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindDescriptorSetGraphicsCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindDescriptorSetComputeCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindDescriptorsGraphicsCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindDescriptorsComputeCommand& command);
    static void CompileCommand(CommandBuffer cmd, const BindDescriptorArenaAllocatorsCommand& command);

    static void CompileCommand(CommandBuffer cmd, const PushConstantsCommand& command);
    
    static void CompileCommand(CommandBuffer cmd, const DrawCommand& command);
    static void CompileCommand(CommandBuffer cmd, const DrawIndexedCommand& command);
    static void CompileCommand(CommandBuffer cmd, const DrawIndexedIndirectCommand& command);
    static void CompileCommand(CommandBuffer cmd, const DrawIndexedIndirectCountCommand& command);

    static void CompileCommand(CommandBuffer cmd, const DispatchCommand& command);
    static void CompileCommand(CommandBuffer cmd, const DispatchIndirectCommand& command);
private:
    static VmaAllocator& Allocator();
    
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

    static Buffer AllocateBuffer(BufferCreateInfo& createInfo, VkBufferUsageFlags usage,
        VmaAllocationCreateFlags allocationFlags);

    static Image CreateImageFromAssetFile(ImageCreateInfo& createInfo, ImageAssetPath assetPath);
    static Image CreateImageFromPixels(ImageCreateInfo& createInfo, Span<const std::byte> pixels);
    static Image CreateImageFromBuffer(ImageCreateInfo& createInfo, Buffer buffer);
    static void PreprocessCreateInfo(ImageCreateInfo& createInfo);
    static Image AllocateImage(ImageCreateInfo& createInfo);
    static VkImageView CreateVulkanImageView(const ImageSubresource& image, VkFormat format);

    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        Span<const Semaphore> semaphores, Span<const PipelineStage> waitStages);
    static std::vector<VkSemaphoreSubmitInfo> CreateVulkanSemaphoreSubmit(
        Span<const TimelineSemaphore> semaphores,
        Span<const u64> waitValues, Span<const PipelineStage> waitStages);

    static void BindDescriptors(CommandBuffer cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout, Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint);
private:
    struct State;
    static State s_State;
};

template <typename Fn>
void Device::ImmediateSubmit(Fn&& uploadFunction)
{
    auto&& [pool, cmd, commandList, fence, queue] = *SubmitContext();

    BeginCommandBuffer(cmd);

    
    uploadFunction(cmd, commandList);
    

    EndCommandBuffer(cmd);
    SubmitCommandBuffer(cmd, queue, fence);
    WaitForFence(fence);
    ResetFence(fence);
    ResetPool(pool);
}
