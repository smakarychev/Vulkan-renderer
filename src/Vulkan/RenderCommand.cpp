#include "RenderCommand.h"

#include "Buffer.h"
#include "CommandBuffer.h"
#include "Driver.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "Swapchain.h"
#include "Syncronization.h"

VkResult RenderCommand::WaitForFence(const Fence& fence)
{
    return vkWaitForFences(Driver::DeviceHandle(), 1, &fence.m_Fence, true, 1'000'000'000);
}

VkResult RenderCommand::ResetFence(const Fence& fence)
{
    return vkResetFences(Driver::DeviceHandle(), 1, &fence.m_Fence);
}

VkResult RenderCommand::AcquireNextImage(const Swapchain& swapchain, const SwapchainFrameSync& swapchainFrameSync, u32& imageIndex)
{
    return vkAcquireNextImageKHR(Driver::DeviceHandle(), swapchain.m_Swapchain, 1'000'000'000, swapchainFrameSync.PresentSemaphore.m_Semaphore, VK_NULL_HANDLE, &imageIndex);
}

VkResult RenderCommand::Present(const Swapchain& swapchain, const QueueInfo& queueInfo, const SwapchainFrameSync& swapchainFrameSync, u32 imageIndex)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &swapchainFrameSync.RenderSemaphore.m_Semaphore;

    return vkQueuePresentKHR(queueInfo.Queue, &presentInfo);
}

VkResult RenderCommand::ResetCommandBuffer(const CommandBuffer& cmd)
{
    return vkResetCommandBuffer(cmd.m_CommandBuffer, 0);
}

VkResult RenderCommand::BeginCommandBuffer(const CommandBuffer& cmd)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    return vkBeginCommandBuffer(cmd.m_CommandBuffer, &beginInfo);
}

VkResult RenderCommand::EndCommandBuffer(const CommandBuffer& cmd)
{
    return vkEndCommandBuffer(cmd.m_CommandBuffer);
}

VkResult RenderCommand::SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, 
    const SwapchainFrameSync& swapchainFrameSync)
{
    VkSubmitInfo  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd.m_CommandBuffer;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &swapchainFrameSync.PresentSemaphore.m_Semaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &swapchainFrameSync.RenderSemaphore.m_Semaphore;

    return vkQueueSubmit(queueInfo.Queue, 1, &submitInfo, swapchainFrameSync.RenderFence.m_Fence);
}

void RenderCommand::BeginRenderPass(const CommandBuffer& cmd, const RenderPass& renderPass,
                                    const Framebuffer& framebuffer, const std::vector<VkClearValue>& clearValues)
{
    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = renderPass.m_RenderPass;
    beginInfo.framebuffer = framebuffer.m_Framebuffer;
    beginInfo.renderArea = {.offset = {0, 0}, .extent = framebuffer.m_Extent};
    beginInfo.clearValueCount = (u32)clearValues.size();
    beginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd.m_CommandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderCommand::EndRenderPass(const CommandBuffer& cmd)
{
    vkCmdEndRenderPass(cmd.m_CommandBuffer);
}

void RenderCommand::BindBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    VkDeviceSize bufferOffset = offset;
    vkCmdBindVertexBuffers(cmd.m_CommandBuffer, 0, 1, &buffer.m_Buffer, &bufferOffset);
}

void RenderCommand::BindPipeline(const CommandBuffer& cmd, const Pipeline& pipeline, VkPipelineBindPoint bindPoint)
{
    vkCmdBindPipeline(cmd.m_CommandBuffer, bindPoint, pipeline.m_Pipeline);
}

void RenderCommand::BindDescriptorSet(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
    const Pipeline& pipeline, VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets)
{
    vkCmdBindDescriptorSets(cmd.m_CommandBuffer,
        bindPoint, pipeline.m_Layout,
        pipeline.FindDescriptorSetLayout(descriptorSet.m_Layout->m_Layout), 1,
        &descriptorSet.m_DescriptorSet,
        (u32)dynamicOffsets.size(), dynamicOffsets.data());
}

void RenderCommand::Draw(const CommandBuffer& cmd, u32 vertexCount)
{
    vkCmdDraw(cmd.m_CommandBuffer, vertexCount, 1, 0, 0);
}

void RenderCommand::Draw(const CommandBuffer& cmd, u32 vertexCount, u32 baseInstance)
{
    vkCmdDraw(cmd.m_CommandBuffer, vertexCount, 1, 0, baseInstance);
}

void RenderCommand::PushConstants(const CommandBuffer& cmd, const Pipeline& pipeline, const void* pushConstants,
                                  const PushConstantDescription& description)
{
    vkCmdPushConstants(cmd.m_CommandBuffer, pipeline.m_Layout, description.m_StageFlags, 0, description.m_SizeBytes, pushConstants);
}

void RenderCommand::SetViewport(const CommandBuffer& cmd, const glm::vec2& size)
{
    VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (f32)size.x, .height = (f32)size.y,
        .minDepth = 0.0f, .maxDepth = 1.0f};

    vkCmdSetViewport(cmd.m_CommandBuffer, 0, 1, &viewport);    
}

void RenderCommand::SetScissors(const CommandBuffer& cmd, const glm::vec2& offset, const glm::vec2& size)
{
    VkRect2D scissor = {
        .offset = {(i32)offset.x, (i32)offset.y},
        .extent = {(u32)size.x, (u32)size.y}};
    
    vkCmdSetScissor(cmd.m_CommandBuffer, 0, 1, &scissor);
}
