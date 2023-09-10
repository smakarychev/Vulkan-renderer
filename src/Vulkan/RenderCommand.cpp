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
    return vkWaitForFences(Driver::DeviceHandle(), 1, &fence.m_Fence, true, 10'000'000'000);
}

VkResult RenderCommand::ResetFence(const Fence& fence)
{
    return vkResetFences(Driver::DeviceHandle(), 1, &fence.m_Fence);
}

VkResult RenderCommand::AcquireNextImage(const Swapchain& swapchain, const SwapchainFrameSync& swapchainFrameSync, u32& imageIndex)
{
    return vkAcquireNextImageKHR(Driver::DeviceHandle(), swapchain.m_Swapchain, 10'000'000'000, swapchainFrameSync.PresentSemaphore.m_Semaphore, VK_NULL_HANDLE, &imageIndex);
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

VkResult RenderCommand::ResetPool(const CommandPool& pool)
{
    return vkResetCommandPool(Driver::DeviceHandle(), pool.m_CommandPool, 0);
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

VkResult RenderCommand::SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const Fence& fence)
{
    VkSubmitInfo  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd.m_CommandBuffer;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.signalSemaphoreCount = 0;

    return vkQueueSubmit(queueInfo.Queue, 1, &submitInfo, fence.m_Fence);
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

void RenderCommand::CopyBuffer(const CommandBuffer& cmd, const Buffer& source, const Buffer& destination)
{
    VkBufferCopy copy = {};
    copy.size = source.GetSizeBytes();
    copy.srcOffset = 0;
    copy.dstOffset = 0;

    vkCmdCopyBuffer(cmd.m_CommandBuffer, source.m_Buffer, destination.m_Buffer, 1, &copy);
}

void RenderCommand::CopyBufferToImage(const CommandBuffer& cmd, const Buffer& source, const Image& destination)
{
    VkImageSubresourceRange range;
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.image = destination.m_ImageData.Image;
    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.subresourceRange = range;
    imageMemoryBarrier.srcAccessMask = 0;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd.m_CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

    VkBufferImageCopy bufferImageCopy = {};
    bufferImageCopy.imageExtent = { destination.m_ImageData.Width, destination.m_ImageData.Height, 1 };
    bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopy.imageSubresource.mipLevel = 0;
    bufferImageCopy.imageSubresource.layerCount = 1;
    bufferImageCopy.imageSubresource.baseArrayLayer = 0;

    vkCmdCopyBufferToImage(cmd.m_CommandBuffer, source.m_Buffer, destination.m_ImageData.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd.m_CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
}

void RenderCommand::BindVertexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    VkDeviceSize bufferOffset = offset;
    vkCmdBindVertexBuffers(cmd.m_CommandBuffer, 0, 1, &buffer.m_Buffer, &bufferOffset);
}

void RenderCommand::BindIndexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    VkDeviceSize bufferOffset = offset;
    vkCmdBindIndexBuffer(cmd.m_CommandBuffer, buffer.m_Buffer, offset, VK_INDEX_TYPE_UINT32);
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

void RenderCommand::DrawIndexed(const CommandBuffer& cmd, u32 indexCount)
{
    vkCmdDrawIndexed(cmd.m_CommandBuffer, indexCount, 1, 0, 0, 0);
}

void RenderCommand::DrawIndexed(const CommandBuffer& cmd, u32 indexCount, u32 baseInstance)
{
    vkCmdDrawIndexed(cmd.m_CommandBuffer, indexCount, 1, 0, 0, baseInstance);
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
