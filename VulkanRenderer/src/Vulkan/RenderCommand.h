#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <vulkan/vulkan_core.h>

#include "CommandBuffer.h"
#include "types.h"
#include "VulkanCommon.h"

class TimelineSemaphore;
class RenderingInfo;
class PipelineLayout;
class Image;
class CommandPool;
struct SwapchainFrameSync;
class DescriptorSet;
class PushConstantDescription;
class Buffer;
class Pipeline;
class CommandBuffer;
class Semaphore;
class Swapchain;
class Fence;

class RenderCommand
{
public:
    static void BeginRendering(const CommandBuffer& cmd, const RenderingInfo& renderingInfo);
    static void EndRendering(const CommandBuffer& cmd);
    
    
    static VkResult WaitForFence(const Fence& fence);
    static VkResult CheckFence(const Fence fence);
    static VkResult ResetFence(const Fence& fence);
    static VkResult AcquireNextImage(const Swapchain& swapchain, const SwapchainFrameSync& swapchainFrameSync, u32& imageIndex);
    static VkResult Present(const Swapchain& swapchain, const QueueInfo& queueInfo, const SwapchainFrameSync& swapchainFrameSync, u32 imageIndex);
    static VkResult ResetPool(const CommandPool& pool);
    static VkResult ResetCommandBuffer(const CommandBuffer& cmd);
    static VkResult BeginCommandBuffer(const CommandBuffer& cmd, VkCommandBufferUsageFlags flags, VkCommandBufferInheritanceInfo* inheritanceInfo);
    static VkResult EndCommandBuffer(const CommandBuffer& cmd);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const BufferSubmitSyncInfo& submitSync);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const BufferSubmitTimelineSyncInfo& submitSync);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const BufferSubmitMixedSyncInfo& submitSync);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const Fence& fence);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const Fence* fence);
    static VkResult SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo, const BufferSubmitSyncInfo& submitSync);
    static VkResult SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo, const BufferSubmitTimelineSyncInfo& submitSync);
    static VkResult SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo, const BufferSubmitMixedSyncInfo& submitSync);


    static void ExecuteSecondaryCommandBuffer(const CommandBuffer& cmd, const CommandBuffer& secondary);

    static void TransitionImage(const CommandBuffer& cmd, const Image& image, const ImageTransitionInfo& transitionInfo);
    static void BlitImage(const CommandBuffer& cmd, const ImageBlitInfo& imageBlitInfo);
    
    static void CopyBuffer(const CommandBuffer& cmd, const Buffer& source, const Buffer& destination, const BufferCopyInfo& bufferCopyInfo);
    static void CopyBufferToImage(const CommandBuffer& cmd, const Buffer& source, const Image& destination);
    
    static void BindVertexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    static void BindVertexBuffers(const CommandBuffer& cmd, const std::vector<Buffer>& buffers, const std::vector<u64>& offsets);
    static void BindIndexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    static void BindPipeline(const CommandBuffer& cmd, const Pipeline& pipeline, VkPipelineBindPoint bindPoint);
    static void BindDescriptorSet(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
        const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets);
    
    static void Draw(const CommandBuffer& cmd, u32 vertexCount);
    static void Draw(const CommandBuffer& cmd, u32 vertexCount, u32 baseInstance);

    static void DrawIndexed(const CommandBuffer& cmd, u32 indexCount);
    static void DrawIndexed(const CommandBuffer& cmd, u32 indexCount, u32 baseInstance);

    static void DrawIndexedIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset, u32 count, u32 stride);
    static void DrawIndexedIndirectCount(const CommandBuffer& cmd, const Buffer& drawBuffer, u64 drawOffset,
        const Buffer& countBuffer, u64 countOffset, u32 maxCount, u32 stride);

    static void Dispatch(const CommandBuffer& cmd, const glm::uvec3& groupSize);
    static void DispatchIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);

    static void PushConstants(const CommandBuffer& cmd, const PipelineLayout& pipelineLayout, const void* pushConstants,
        const PushConstantDescription& description);

    static void CreateBarrier(const CommandBuffer& cmd, const PipelineBarrierInfo& pipelineBarrierInfo);
    static void CreateBarrier(const CommandBuffer& cmd, const PipelineBufferBarrierInfo& pipelineBarrierInfo);
    static void CreateBarrier(const CommandBuffer& cmd, const PipelineImageBarrierInfo& pipelineBarrierInfo);

    static void BeginConditionalRendering(const CommandBuffer& cmd, const Buffer& conditionalBuffer, u64 offset);
    static void EndConditionalRendering(const CommandBuffer& cmd);
    
    static void SetViewport(const CommandBuffer& cmd, const glm::vec2& size);
    static void SetScissors(const CommandBuffer& cmd, const glm::vec2& offset, const glm::vec2& size);
};
