#pragma once

#include "DeviceFreelist.h"
#include "Core/ProfilerContext.h"

#include "Rendering/CommandBuffer.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "Rendering/RenderingInfo.h"
#include "Rendering/Swapchain.h"
#include "Rendering/Synchronization.h"

#include <functional>

#include "DeviceSparseSet.h"
#include "imgui/imgui.h"
#include "Rendering/Buffer/BufferArena.h"
#include "Rendering/Commands/RenderCommandList.h"

struct FrameContext;
class DeviceResources;
struct CopyBufferCommand;
struct CopyBufferToImageCommand;
class ProfilerContext;
struct GLFWwindow;

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

class Device
{
    friend class DeviceInternal;
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
    template <typename T>
    static Span<const T> GetMappedBufferView(const BufferSubresource& buffer);
    static const BufferDescription& GetBufferDescription(Buffer buffer);
    static u64 GetDeviceAddress(Buffer buffer);

    static BufferArena CreateBufferArena(BufferArenaCreateInfo&& createInfo, 
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(BufferArena arena);
    static void ResizeBufferArenaPhysical(BufferArena arena, u64 newSize, RenderCommandList& cmdList,
        bool copyData = true);
    static Buffer GetBufferArenaUnderlyingBuffer(BufferArena arena);
    static u64 GetBufferArenaSizeBytesPhysical(BufferArena arena);
    static BufferSuballocationResult BufferArenaSuballocate(BufferArena arena, u64 sizeBytes,
        u32 alignment = 8);
    static void BufferArenaFree(BufferArena arena, const BufferSuballocation& suballocation);
    
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
    static DescriptorsLayout GetEmptyDescriptorsLayout();
    static void Destroy(DescriptorsLayout layout);
    
    static DescriptorArenaAllocator CreateDescriptorArenaAllocator(DescriptorArenaAllocatorCreateInfo&& createInfo,
        DeletionQueue& deletionQueue = DeletionQueue());
    static void Destroy(DescriptorArenaAllocator allocator);
    static std::optional<Descriptors> AllocateDescriptors(DescriptorArenaAllocator allocator,
        DescriptorsLayout layout, const DescriptorAllocatorAllocationBindings& bindings);
    static void ResetDescriptorArenaAllocator(DescriptorArenaAllocator allocator);
    
    static void UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
        const BufferSubresource& buffer, u32 index);  
    static void UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo, Sampler sampler);  
    static void UpdateDescriptors(Descriptors descriptors, DescriptorSlotInfo slotInfo,
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
    static ImmediateSubmitContext GetSubmitContext();
    static void FreeSubmitContext(const ImmediateSubmitContext& ctx);
    
    static ProfilerContext::Ctx CreateTracyGraphicsContext(CommandBuffer cmd);
    static void DestroyTracyGraphicsContext(ProfilerContext::Ctx context);
    static void CreateGpuProfileFrame(ProfilerScopedZoneGpu& zoneGpu, const SourceLocationData& sourceLocationData);
    static void DestroyGpuProfileFrame(ProfilerScopedZoneGpu& zoneGpu);
    static void CollectGpuProfileFrames();

    static ImTextureID CreateImGuiImage(const ImageSubresource& texture, Sampler sampler, ImageLayout layout);
    static void DestroyImGuiImage(ImTextureID image);

    static void DumpMemoryStats(const std::filesystem::path& path);

    static void BeginCommandBufferLabel(CommandBuffer cmd, std::string_view label);
    static void EndCommandBufferLabel(CommandBuffer cmd);
    static void NameBuffer(Buffer buffer, std::string_view name);
    static void NameImage(Image image, std::string_view name);
    static void NamePipeline(Pipeline pipeline, std::string_view name);
    
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
    static DeviceResources& Resources();
    static void ShutdownResources();

    static void InitImGuiUI();
    static void ShutdownImGuiUI();


    static void CreateInstance(const DeviceCreateInfo& createInfo);
    static void CreateSurface(const DeviceCreateInfo& createInfo);
    static void ChooseGPU(const DeviceCreateInfo& createInfo);
    static void CreateDevice(const DeviceCreateInfo& createInfo);
    static void RetrieveDeviceQueues();
    static void CreateDebugUtilsMessenger();
    static void DestroyDebugUtilsMessenger();

    static void CreateSwapchainImages(Swapchain swapchain);
    static void DestroySwapchainImages(Swapchain swapchain);

    static Image CreateImageFromAssetFile(ImageCreateInfo& createInfo, ImageAssetPath assetPath);
    static Image CreateImageFromPixels(ImageCreateInfo& createInfo, Span<const std::byte> pixels);
    static Image CreateImageFromBuffer(ImageCreateInfo& createInfo, Buffer buffer);
    static void PreprocessCreateInfo(ImageCreateInfo& createInfo);
    static Image AllocateImage(ImageCreateInfo& createInfo);
private:
    struct State;
    static State s_State;
};

template <typename T>
Span<const T> Device::GetMappedBufferView(const BufferSubresource& buffer)
{
    return Span<const T>((const T*)((const u8*)GetBufferMappedAddress(buffer.Buffer) + buffer.Description.Offset),
        buffer.Description.SizeBytes / sizeof(T));
}

template <typename Fn>
void Device::ImmediateSubmit(Fn&& uploadFunction)
{
    auto ctx = GetSubmitContext();
    uploadFunction(ctx.CommandList);
    FreeSubmitContext(ctx);
}
