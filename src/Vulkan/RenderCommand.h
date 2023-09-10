#pragma once

#include <glm/vec2.hpp>
#include <vulkan/vulkan_core.h>

#include "types.h"
#include "VulkanCommon.h"

class Image;
class CommandPool;
struct SwapchainFrameSync;
class DescriptorSet;
class PushConstantDescription;
class Buffer;
class Pipeline;
class Framebuffer;
class RenderPass;
class CommandBuffer;
class Semaphore;
class Swapchain;
class Fence;

class RenderCommand
{
public:
    static VkResult WaitForFence(const Fence& fence);
    static VkResult ResetFence(const Fence& fence);
    static VkResult AcquireNextImage(const Swapchain& swapchain, const SwapchainFrameSync& swapchainFrameSync, u32& imageIndex);
    static VkResult Present(const Swapchain& swapchain, const QueueInfo& queueInfo, const SwapchainFrameSync& swapchainFrameSync, u32 imageIndex);
    static VkResult ResetPool(const CommandPool& pool);
    static VkResult ResetCommandBuffer(const CommandBuffer& cmd);
    static VkResult BeginCommandBuffer(const CommandBuffer& cmd);
    static VkResult EndCommandBuffer(const CommandBuffer& cmd);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const SwapchainFrameSync& swapchainFrameSync);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const Fence& fence);

    static void BeginRenderPass(const CommandBuffer& cmd, const RenderPass& renderPass,
        const Framebuffer& framebuffer, const std::vector<VkClearValue>& clearValues);
    static void EndRenderPass(const CommandBuffer& cmd);

    static void CopyBuffer(const CommandBuffer& cmd, const Buffer& source, const Buffer& destination);
    static void CopyBufferToImage(const CommandBuffer& cmd, const Buffer& source, const Image& destination);
    
    static void BindVertexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    static void BindIndexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset);
    static void BindPipeline(const CommandBuffer& cmd, const Pipeline& pipeline, VkPipelineBindPoint bindPoint);
    static void BindDescriptorSet(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
        const Pipeline& pipeline, VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets);
    
    static void Draw(const CommandBuffer& cmd, u32 vertexCount);
    static void Draw(const CommandBuffer& cmd, u32 vertexCount, u32 baseInstance);

    static void DrawIndexed(const CommandBuffer& cmd, u32 indexCount);
    static void DrawIndexed(const CommandBuffer& cmd, u32 indexCount, u32 baseInstance);

    static void PushConstants(const CommandBuffer& cmd, const Pipeline& pipeline, const void* pushConstants,
        const PushConstantDescription& description);
    
    static void SetViewport(const CommandBuffer& cmd, const glm::vec2& size);
    static void SetScissors(const CommandBuffer& cmd, const glm::vec2& offset, const glm::vec2& size);
};
