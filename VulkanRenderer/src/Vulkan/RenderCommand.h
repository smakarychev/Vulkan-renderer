#pragma once

#include <Vulkan/vulkan_core.h>

#include "Rendering/CommandBuffer.h"
#include "Rendering/Image/ImageTraits.h"
#include "types.h"
#include "Rendering/Pipeline.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/RenderingInfo.h"
#include "Rendering/Swapchain.h"

class DescriptorArenaAllocators;
struct ImageSubresource;
struct ImageBlitInfo;
class Image;
struct SwapchainFrameSync;
struct PushConstantDescription;

class RenderCommand
{
public:
    static void BeginRendering(CommandBuffer cmd, RenderingInfo renderingInfo);
    static void EndRendering(CommandBuffer cmd);

    static void PrepareSwapchainPresent(CommandBuffer cmd, Swapchain swapchain, u32 imageIndex);
    
    static void ExecuteSecondaryCommandBuffer(CommandBuffer cmd, CommandBuffer secondary);

    using ImageCopyInfo = ImageBlitInfo;
    static void CopyImage(CommandBuffer cmd, const ImageCopyInfo& source, const ImageCopyInfo& destination);
    static void BlitImage(CommandBuffer cmd,
        const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter);
    
    static void CopyBuffer(CommandBuffer cmd, Buffer source, Buffer destination,
        const BufferCopyInfo& bufferCopyInfo);
    static void CopyBufferToImage(CommandBuffer cmd, Buffer source, const ImageSubresource& destination);
    
    static void BindVertexBuffer(CommandBuffer cmd, Buffer buffer, u64 offset);
    static void BindVertexBuffers(CommandBuffer cmd, const std::vector<Buffer>& buffers,
        const std::vector<u64>& offsets);
    static void BindIndexU32Buffer(CommandBuffer cmd, Buffer buffer, u64 offset);
    static void BindIndexU16Buffer(CommandBuffer cmd, Buffer buffer, u64 offset);
    static void BindIndexU8Buffer(CommandBuffer cmd, Buffer buffer, u64 offset);
    
    static void BindGraphics(CommandBuffer cmd, Pipeline pipeline);
    static void BindCompute(CommandBuffer cmd, Pipeline pipeline);
    static void BindGraphics(CommandBuffer cmd, DescriptorSet descriptorSet,
        PipelineLayout pipelineLayout, u32 setIndex, const std::vector<u32>& dynamicOffsets);
    static void BindCompute(CommandBuffer cmd, DescriptorSet descriptorSet,
        PipelineLayout pipelineLayout, u32 setIndex, const std::vector<u32>& dynamicOffsets);
    static void BindGraphicsImmutableSamplers(CommandBuffer cmd,
        PipelineLayout pipelineLayout, u32 setIndex);
    static void BindComputeImmutableSamplers(CommandBuffer cmd,
        PipelineLayout pipelineLayout, u32 setIndex);

    /* `bufferIndex` is usually a frame number from frame context (between 0 and BUFFERED_FRAMES)
     * NOTE: usually you want to call `Bind` on `DescriptorArenaAllocators`
     */
    static void Bind(CommandBuffer cmd, DescriptorArenaAllocator allocator, u32 bufferIndex);
    /* `bufferIndex` is usually a frame number from frame context (between 0 and BUFFERED_FRAMES) */
    static void Bind(CommandBuffer cmd, const DescriptorArenaAllocators& allocators, u32 bufferIndex);
    static void BindGraphics(CommandBuffer cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, Descriptors descriptors, u32 firstSet);
    static void BindCompute(CommandBuffer cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, Descriptors descriptors, u32 firstSet);

    
    static void Draw(CommandBuffer cmd, u32 vertexCount);
    static void Draw(CommandBuffer cmd, u32 vertexCount, u32 baseInstance);

    static void DrawIndexed(CommandBuffer cmd, u32 indexCount);
    static void DrawIndexed(CommandBuffer cmd, u32 indexCount, u32 baseInstance);

    static void DrawIndexedIndirect(CommandBuffer cmd, Buffer buffer, u64 offset, u32 count,
        u32 stride = sizeof(IndirectDrawCommand));
    static void DrawIndexedIndirectCount(CommandBuffer cmd, Buffer drawBuffer, u64 drawOffset,
        Buffer countBuffer, u64 countOffset, u32 maxCount, u32 stride = sizeof(IndirectDrawCommand));

    static void Dispatch(CommandBuffer cmd, const glm::uvec3& groupSize);
    static void Dispatch(CommandBuffer cmd, const glm::uvec3& invocations, const glm::uvec3& workGroups);
    static void DispatchIndirect(CommandBuffer cmd, Buffer buffer, u64 offset);

    static void PushConstants(CommandBuffer cmd, PipelineLayout pipelineLayout, const void* pushConstants);
    template <typename T>
    static void PushConstants(CommandBuffer cmd, PipelineLayout pipelineLayout, const T& pushConstants)
    {
        PushConstants(cmd, pipelineLayout, (const void*)&pushConstants);
    }

    static void WaitOnFullPipelineBarrier(CommandBuffer cmd);
    static void WaitOnBarrier(CommandBuffer cmd, DependencyInfo dependencyInfo);
    static void SignalSplitBarrier(CommandBuffer cmd, SplitBarrier splitBarrier, DependencyInfo dependencyInfo);
    static void WaitOnSplitBarrier(CommandBuffer cmd, SplitBarrier splitBarrier, DependencyInfo dependencyInfo);
    static void ResetSplitBarrier(CommandBuffer cmd, SplitBarrier splitBarrier, DependencyInfo dependencyInfo);
    
    static void BeginConditionalRendering(CommandBuffer cmd, Buffer conditionalBuffer, u64 offset);
    static void EndConditionalRendering(CommandBuffer cmd);
    
    static void SetViewport(CommandBuffer cmd, const glm::vec2& size);
    static void SetScissors(CommandBuffer cmd, const glm::vec2& offset, const glm::vec2& size);
    static void SetDepthBias(CommandBuffer cmd, const DepthBias& depthBias);

    static void ImGuiBeginFrame();
    static void DrawImGui(CommandBuffer cmd, const RenderingInfo renderingInfo);
private:
    static void BindDescriptors(CommandBuffer cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, Descriptors descriptors, u32 firstSet, VkPipelineBindPoint bindPoint);
};
