#pragma once

#include <Vulkan/vulkan_core.h>

#include "Rendering/CommandBuffer.h"
#include "Rendering/Image/ImageTraits.h"
#include "types.h"
#include "Rendering/Pipeline.h"
#include "Rendering/RenderingCommon.h"

class DescriptorArenaAllocators;
class Descriptors;
class DescriptorArenaAllocator;
struct ImageSubresource;
struct ImageBlitInfo;
class SplitBarrier;
class DependencyInfo;
class TimelineSemaphore;
class RenderingInfo;
class Image;
class CommandPool;
struct SwapchainFrameSync;
class DescriptorSet;
struct PushConstantDescription;
class Buffer;
class CommandBuffer;
class Semaphore;
class Swapchain;
class Fence;

class RenderCommand
{
public:
    static void BeginRendering(const CommandBuffer& cmd, const RenderingInfo& renderingInfo);
    static void EndRendering(const CommandBuffer& cmd);


    static void ExecuteSecondaryCommandBuffer(const CommandBuffer& cmd, const CommandBuffer& secondary);

    using ImageCopyInfo = ImageBlitInfo;
    static void CopyImage(const CommandBuffer& cmd, const ImageCopyInfo& source, const ImageCopyInfo& destination);
    static void BlitImage(const CommandBuffer& cmd,
        const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter);
    
    static void CopyBuffer(const CommandBuffer& cmd, const Buffer& source, const Buffer& destination,
        const BufferCopyInfo& bufferCopyInfo);
    static void CopyBufferToImage(const CommandBuffer& cmd, const Buffer& source, const ImageSubresource& destination);
    
    static void BindVertexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    static void BindVertexBuffers(const CommandBuffer& cmd, const std::vector<Buffer>& buffers,
        const std::vector<u64>& offsets);
    static void BindIndexU32Buffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    static void BindIndexU16Buffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    static void BindIndexU8Buffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    
    static void BindGraphics(const CommandBuffer& cmd, Pipeline pipeline);
    static void BindCompute(const CommandBuffer& cmd, Pipeline pipeline);
    static void BindGraphics(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
        PipelineLayout pipelineLayout, u32 setIndex, const std::vector<u32>& dynamicOffsets);
    static void BindCompute(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
        PipelineLayout pipelineLayout, u32 setIndex, const std::vector<u32>& dynamicOffsets);
    static void BindGraphicsImmutableSamplers(const CommandBuffer& cmd,
        PipelineLayout pipelineLayout, u32 setIndex);
    static void BindComputeImmutableSamplers(const CommandBuffer& cmd,
        PipelineLayout pipelineLayout, u32 setIndex);

    static void Bind(const CommandBuffer& cmd, const DescriptorArenaAllocator& allocator);
    static void Bind(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators);
    static void BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, const Descriptors& descriptors, u32 firstSet);
    static void BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, const Descriptors& descriptors, u32 firstSet);

    
    static void Draw(const CommandBuffer& cmd, u32 vertexCount);
    static void Draw(const CommandBuffer& cmd, u32 vertexCount, u32 baseInstance);

    static void DrawIndexed(const CommandBuffer& cmd, u32 indexCount);
    static void DrawIndexed(const CommandBuffer& cmd, u32 indexCount, u32 baseInstance);

    static void DrawIndexedIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset, u32 count,
        u32 stride = sizeof(IndirectDrawCommand));
    static void DrawIndexedIndirectCount(const CommandBuffer& cmd, const Buffer& drawBuffer, u64 drawOffset,
        const Buffer& countBuffer, u64 countOffset, u32 maxCount, u32 stride = sizeof(IndirectDrawCommand));

    static void Dispatch(const CommandBuffer& cmd, const glm::uvec3& groupSize);
    static void Dispatch(const CommandBuffer& cmd, const glm::uvec3& invocations, const glm::uvec3& workGroups);
    static void DispatchIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);

    static void PushConstants(const CommandBuffer& cmd, PipelineLayout pipelineLayout, const void* pushConstants);
    template <typename T>
    static void PushConstants(const CommandBuffer& cmd, PipelineLayout pipelineLayout, const T& pushConstants)
    {
        PushConstants(cmd, pipelineLayout, (const void*)&pushConstants);
    }

    static void WaitOnFullPipelineBarrier(const CommandBuffer& cmd);
    static void WaitOnBarrier(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo);
    static void SignalSplitBarrier(const CommandBuffer& cmd, const SplitBarrier& splitBarrier,
        const DependencyInfo& dependencyInfo);
    static void WaitOnSplitBarrier(const CommandBuffer& cmd, const SplitBarrier& splitBarrier,
        const DependencyInfo& dependencyInfo);
    static void ResetSplitBarrier(const CommandBuffer& cmd, const SplitBarrier& splitBarrier,
        const DependencyInfo& dependencyInfo);
    
    static void BeginConditionalRendering(const CommandBuffer& cmd, const Buffer& conditionalBuffer, u64 offset);
    static void EndConditionalRendering(const CommandBuffer& cmd);
    
    static void SetViewport(const CommandBuffer& cmd, const glm::vec2& size);
    static void SetScissors(const CommandBuffer& cmd, const glm::vec2& offset, const glm::vec2& size);
    static void SetDepthBias(const CommandBuffer& cmd, const DepthBias& depthBias);

    static void ImGuiBeginFrame();
    static void DrawImGui(const CommandBuffer& cmd, const RenderingInfo& renderingInfo);
private:
    static void BindDescriptors(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, const Descriptors& descriptors, u32 firstSet, VkPipelineBindPoint bindPoint);
};
