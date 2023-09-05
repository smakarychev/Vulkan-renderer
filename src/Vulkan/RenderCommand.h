#pragma once

#include <glm/vec2.hpp>
#include <vulkan/vulkan_core.h>

#include "CommandBuffer.h"
#include "types.h"
#include "VulkanCommon.h"

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
    static VkResult AcquireNextImage(const Swapchain& swapchain, const Semaphore& semaphore, u32& imageIndex);
    static VkResult Present(const Swapchain& swapchain, const QueueInfo& queueInfo, const Semaphore& semaphore, u32 imageIndex);
    static VkResult ResetCommandBuffer(const CommandBuffer& cmd);
    static VkResult BeginCommandBuffer(const CommandBuffer& cmd);
    static VkResult EndCommandBuffer(const CommandBuffer& cmd);
    static VkResult SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const SwapchainFrameSync& swapchainFrameSync);
    // todo: state for clear color?
    static void BeginRenderPass(const CommandBuffer& cmd, const RenderPass& renderPass,
        const Framebuffer& framebuffer, const std::vector<VkClearValue>& clearValues);
    static void EndRenderPass(const CommandBuffer& cmd);

    static void BindPipeline(const CommandBuffer& cmd, const Pipeline& pipeline, VkPipelineBindPoint bindPoint);

    // todo: actual implementation
    static void Draw(const CommandBuffer& cmd);
    
    static void SetViewport(const CommandBuffer& cmd, const glm::vec2& size);
    static void SetScissors(const CommandBuffer& cmd, const glm::vec2& offset, const glm::vec2& size);
};
