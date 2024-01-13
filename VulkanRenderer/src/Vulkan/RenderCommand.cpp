#include "RenderCommand.h"

#include <algorithm>
#include <ranges>

#include "Buffer.h"
#include "CommandBuffer.h"
#include "Driver.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Syncronization.h"

void RenderCommand::BeginRendering(const CommandBuffer& cmd, const RenderingInfo& renderingInfo)
{
    vkCmdBeginRendering(cmd.m_CommandBuffer, &renderingInfo.m_RenderingInfo);
}

void RenderCommand::EndRendering(const CommandBuffer& cmd)
{
    vkCmdEndRendering(cmd.m_CommandBuffer);
}

VkResult RenderCommand::WaitForFence(const Fence& fence)
{
    return vkWaitForFences(Driver::DeviceHandle(), 1, &fence.m_Fence, true, 10'000'000'000);
}

VkResult RenderCommand::CheckFence(const Fence fence)
{
    return vkGetFenceStatus(Driver::DeviceHandle(), fence.m_Fence);
}

VkResult RenderCommand::ResetFence(const Fence& fence)
{
    return vkResetFences(Driver::DeviceHandle(), 1, &fence.m_Fence);
}

VkResult RenderCommand::AcquireNextImage(const Swapchain& swapchain,
    const SwapchainFrameSync& swapchainFrameSync, u32& imageIndex)
{
    return vkAcquireNextImageKHR(Driver::DeviceHandle(), swapchain.m_Swapchain, 10'000'000'000,
        swapchainFrameSync.PresentSemaphore.m_Semaphore, VK_NULL_HANDLE, &imageIndex);
}

VkResult RenderCommand::Present(const Swapchain& swapchain, const QueueInfo& queueInfo,
    const SwapchainFrameSync& swapchainFrameSync, u32 imageIndex)
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

VkResult RenderCommand::BeginCommandBuffer(const CommandBuffer& cmd, VkCommandBufferUsageFlags flags,
    VkCommandBufferInheritanceInfo* inheritanceInfo)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pInheritanceInfo = inheritanceInfo;
    beginInfo.flags = flags;

    return vkBeginCommandBuffer(cmd.m_CommandBuffer, &beginInfo);
}

VkResult RenderCommand::EndCommandBuffer(const CommandBuffer& cmd)
{
    return vkEndCommandBuffer(cmd.m_CommandBuffer);
}

VkResult RenderCommand::SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo,
    const BufferSubmitSyncInfo& submitSync)
{
    return SubmitCommandBuffers({cmd}, queueInfo, submitSync);
}

VkResult RenderCommand::SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    return SubmitCommandBuffers({cmd}, queueInfo, submitSync);
}

VkResult RenderCommand::SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo,
    const BufferSubmitMixedSyncInfo& submitSync)
{
    return SubmitCommandBuffers({cmd}, queueInfo, submitSync);
}

VkResult RenderCommand::SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const Fence& fence)
{
    return  SubmitCommandBuffer(cmd, queueInfo, &fence);
}

VkResult RenderCommand::SubmitCommandBuffer(const CommandBuffer& cmd, const QueueInfo& queueInfo, const Fence* fence)
{
    VkSubmitInfo  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd.m_CommandBuffer;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.signalSemaphoreCount = 0;

    return vkQueueSubmit(queueInfo.Queue, 1, &submitInfo, fence ? fence->m_Fence : VK_NULL_HANDLE);
}

VkResult RenderCommand::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo,
    const BufferSubmitSyncInfo& submitSync)
{
    auto semaphoreToVkSemaphore = [](const Semaphore* semaphore) { return semaphore->m_Semaphore; };
    auto bufferToVkBuffer = [](const CommandBuffer& buffer) { return buffer.m_CommandBuffer; };
    std::vector<VkSemaphore> waitSemaphores;
    waitSemaphores.reserve(submitSync.WaitSemaphores.size());
    std::vector<VkSemaphore> signalSemaphores;
    signalSemaphores.reserve(submitSync.SignalSemaphores.size());
    std::vector<VkCommandBuffer> buffers;
    buffers.reserve(cmds.size());
    std::transform(submitSync.WaitSemaphores.begin(), submitSync.WaitSemaphores.end(), std::back_inserter(waitSemaphores), semaphoreToVkSemaphore);
    std::transform(submitSync.SignalSemaphores.begin(), submitSync.SignalSemaphores.end(), std::back_inserter(signalSemaphores), semaphoreToVkSemaphore);
    std::transform(cmds.begin(), cmds.end(), std::back_inserter(buffers), bufferToVkBuffer);
    
    VkSubmitInfo  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = submitSync.WaitStages.data();
    submitInfo.commandBufferCount = (u32)buffers.size();
    submitInfo.pCommandBuffers = buffers.data();
    submitInfo.waitSemaphoreCount = (u32)waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.signalSemaphoreCount = (u32)signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    return vkQueueSubmit(queueInfo.Queue, 1, &submitInfo, submitSync.Fence ? submitSync.Fence->m_Fence : VK_NULL_HANDLE);
}

VkResult RenderCommand::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        submitSync.SignalSemaphores[i]->SetTimeline(submitSync.SignalValues[i]);
    
    auto semaphoreToVkSemaphore = [](const TimelineSemaphore* semaphore) { return semaphore->m_Semaphore; };
    auto bufferToVkBuffer = [](const CommandBuffer& buffer) { return buffer.m_CommandBuffer; };
    std::vector<VkSemaphore> waitSemaphores;
    waitSemaphores.reserve(submitSync.WaitSemaphores.size());
    std::vector<VkSemaphore> signalSemaphores;
    signalSemaphores.reserve(submitSync.SignalSemaphores.size());
    std::vector<VkCommandBuffer> buffers;
    buffers.reserve(cmds.size());
    std::transform(submitSync.WaitSemaphores.begin(), submitSync.WaitSemaphores.end(), std::back_inserter(waitSemaphores), semaphoreToVkSemaphore);
    std::transform(submitSync.SignalSemaphores.begin(), submitSync.SignalSemaphores.end(), std::back_inserter(signalSemaphores), semaphoreToVkSemaphore);
    std::transform(cmds.begin(), cmds.end(), std::back_inserter(buffers), bufferToVkBuffer);

    VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo = {};
    timelineSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
    timelineSemaphoreSubmitInfo.waitSemaphoreValueCount = (u32)submitSync.WaitValues.size();
    timelineSemaphoreSubmitInfo.pWaitSemaphoreValues = submitSync.WaitValues.data();
    timelineSemaphoreSubmitInfo.signalSemaphoreValueCount = (u32)submitSync.SignalValues.size();
    timelineSemaphoreSubmitInfo.pSignalSemaphoreValues = submitSync.SignalValues.data();

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = submitSync.WaitStages.data();
    submitInfo.commandBufferCount = (u32)buffers.size();
    submitInfo.pCommandBuffers = buffers.data();
    submitInfo.waitSemaphoreCount = (u32)waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.signalSemaphoreCount = (u32)signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();
    
    submitInfo.pNext = &timelineSemaphoreSubmitInfo;

    return vkQueueSubmit(queueInfo.Queue, 1, &submitInfo, submitSync.Fence ? submitSync.Fence->m_Fence : VK_NULL_HANDLE);
}

VkResult RenderCommand::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo,
    const BufferSubmitMixedSyncInfo& submitSync)
{
    for (u32 i = 0; i < submitSync.SignalTimelineSemaphores.size(); i++)
        submitSync.SignalTimelineSemaphores[i]->SetTimeline(submitSync.SignalValues[i]);
    
    auto timelineSemaphoreToVkSemaphore = [](const TimelineSemaphore* semaphore) { return semaphore->m_Semaphore; };
    auto semaphoreToVkSemaphore = [](const Semaphore* semaphore) { return semaphore->m_Semaphore; };
    auto bufferToVkBuffer = [](const CommandBuffer& buffer) { return buffer.m_CommandBuffer; };
    std::vector<VkSemaphore> waitSemaphores;
    waitSemaphores.reserve(submitSync.WaitSemaphores.size() + submitSync.WaitTimelineSemaphores.size());
    std::vector<VkSemaphore> signalSemaphores;
    signalSemaphores.reserve(submitSync.SignalSemaphores.size() + submitSync.SignalTimelineSemaphores.size());
    std::vector<VkCommandBuffer> buffers;
    buffers.reserve(cmds.size());
    std::transform(submitSync.WaitSemaphores.begin(), submitSync.WaitSemaphores.end(), std::back_inserter(waitSemaphores), semaphoreToVkSemaphore);
    std::transform(submitSync.WaitTimelineSemaphores.begin(), submitSync.WaitTimelineSemaphores.end(), std::back_inserter(waitSemaphores), timelineSemaphoreToVkSemaphore);
    std::transform(submitSync.SignalSemaphores.begin(), submitSync.SignalSemaphores.end(), std::back_inserter(signalSemaphores), semaphoreToVkSemaphore);
    std::transform(submitSync.SignalTimelineSemaphores.begin(), submitSync.SignalTimelineSemaphores.end(), std::back_inserter(signalSemaphores), timelineSemaphoreToVkSemaphore);
    std::transform(cmds.begin(), cmds.end(), std::back_inserter(buffers), bufferToVkBuffer);

    std::vector<u64> waitValues, signalValues;
    waitValues.reserve(waitSemaphores.size());
    signalValues.reserve(signalSemaphores.size());
    for (u32 i = 0; i < waitSemaphores.size(); i++)
        waitValues.push_back(i < submitSync.WaitSemaphores.size() ? 0 : submitSync.WaitValues[i - submitSync.WaitSemaphores.size()]);
    for (u32 i = 0; i < signalSemaphores.size(); i++)
        signalValues.push_back(i < submitSync.SignalSemaphores.size() ? 0 : submitSync.SignalValues[i - submitSync.SignalSemaphores.size()]);

    VkTimelineSemaphoreSubmitInfo timelineSemaphoreSubmitInfo = {};
    timelineSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR;
    timelineSemaphoreSubmitInfo.waitSemaphoreValueCount = (u32)waitValues.size();
    timelineSemaphoreSubmitInfo.pWaitSemaphoreValues = waitValues.data();
    timelineSemaphoreSubmitInfo.signalSemaphoreValueCount = (u32)signalValues.size();
    timelineSemaphoreSubmitInfo.pSignalSemaphoreValues = signalValues.data();

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = submitSync.WaitStages.data();
    submitInfo.commandBufferCount = (u32)buffers.size();
    submitInfo.pCommandBuffers = buffers.data();
    submitInfo.waitSemaphoreCount = (u32)waitSemaphores.size();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.signalSemaphoreCount = (u32)signalSemaphores.size();
    submitInfo.pSignalSemaphores = signalSemaphores.data();
    
    submitInfo.pNext = &timelineSemaphoreSubmitInfo;

    return vkQueueSubmit(queueInfo.Queue, 1, &submitInfo, submitSync.Fence ? submitSync.Fence->m_Fence : VK_NULL_HANDLE);
}

void RenderCommand::ExecuteSecondaryCommandBuffer(const CommandBuffer& cmd, const CommandBuffer& secondary)
{
    vkCmdExecuteCommands(cmd.m_CommandBuffer, 1, &secondary.m_CommandBuffer);
}

void RenderCommand::TransitionImage(const CommandBuffer& cmd, const Image& image,
    const ImageTransitionInfo& transitionInfo)
{
    vkCmdPipelineBarrier(cmd.m_CommandBuffer, transitionInfo.SourceStage, transitionInfo.DestinationStage, 0,
        0, nullptr, 0, nullptr, 1, &transitionInfo.MemoryBarrier);
}

void RenderCommand::BlitImage(const CommandBuffer& cmd, const ImageBlitInfo& imageBlitInfo)
{
    vkCmdBlitImage(cmd.m_CommandBuffer,
        imageBlitInfo.SourceImage->m_ImageData.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        imageBlitInfo.DestinationImage->m_ImageData.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, imageBlitInfo.ImageBlit,
        imageBlitInfo.Filter);
}

void RenderCommand::CopyBuffer(const CommandBuffer& cmd, const Buffer& source, const Buffer& destination, const BufferCopyInfo& bufferCopyInfo)
{
    VkBufferCopy copy = {};
    copy.size = bufferCopyInfo.SizeBytes;
    copy.srcOffset = bufferCopyInfo.SourceOffset;
    copy.dstOffset = bufferCopyInfo.DestinationOffset;

    vkCmdCopyBuffer(cmd.m_CommandBuffer, source.m_Buffer, destination.m_Buffer, 1, &copy);
}

void RenderCommand::CopyBufferToImage(const CommandBuffer& cmd, const Buffer& source, const Image& destination)
{
    VkBufferImageCopy bufferImageCopy = {};
    bufferImageCopy.imageExtent = { destination.m_ImageData.Width, destination.m_ImageData.Height, 1 };
    bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferImageCopy.imageSubresource.mipLevel = 0;
    bufferImageCopy.imageSubresource.layerCount = 1;
    bufferImageCopy.imageSubresource.baseArrayLayer = 0;

    vkCmdCopyBufferToImage(cmd.m_CommandBuffer, source.m_Buffer, destination.m_ImageData.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);
}

void RenderCommand::BindVertexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdBindVertexBuffers(cmd.m_CommandBuffer, 0, 1, &buffer.m_Buffer, &offset);
}

void RenderCommand::BindVertexBuffers(const CommandBuffer& cmd, const std::vector<Buffer>& buffers,
    const std::vector<u64>& offsets)
{
    std::vector<VkBuffer> vkBuffers(buffers.size());
    for (u32 i = 0; i < vkBuffers.size(); i++)
        vkBuffers[i] = buffers[i].m_Buffer;
    
    vkCmdBindVertexBuffers(cmd.m_CommandBuffer, 0, (u32)vkBuffers.size(), vkBuffers.data(), offsets.data());
}

void RenderCommand::BindIndexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdBindIndexBuffer(cmd.m_CommandBuffer, buffer.m_Buffer, offset, VK_INDEX_TYPE_UINT8_EXT);
}

void RenderCommand::BindPipeline(const CommandBuffer& cmd, const Pipeline& pipeline, VkPipelineBindPoint bindPoint)
{
    vkCmdBindPipeline(cmd.m_CommandBuffer, bindPoint, pipeline.m_Pipeline);
}

void RenderCommand::BindDescriptorSet(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
    const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets)
{
    vkCmdBindDescriptorSets(cmd.m_CommandBuffer,
        bindPoint, pipelineLayout.m_Layout,
        setIndex, 1,
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

void RenderCommand::DrawIndexedIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset, u32 count,
    u32 stride)
{
    vkCmdDrawIndexedIndirect(cmd.m_CommandBuffer, buffer.m_Buffer, offset, count, stride);    
}

void RenderCommand::DrawIndexedIndirectCount(const CommandBuffer& cmd, const Buffer& drawBuffer, u64 drawOffset,
    const Buffer& countBuffer, u64 countOffset, u32 maxCount, u32 stride)
{
    vkCmdDrawIndexedIndirectCount(cmd.m_CommandBuffer,
        drawBuffer.m_Buffer, drawOffset,
        countBuffer.m_Buffer, countOffset,
        maxCount, stride);
}

void RenderCommand::Dispatch(const CommandBuffer& cmd, const glm::uvec3& groupSize)
{
    vkCmdDispatch(cmd.m_CommandBuffer, groupSize.x, groupSize.y, groupSize.z);
}

void RenderCommand::DispatchIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdDispatchIndirect(cmd.m_CommandBuffer, buffer.m_Buffer, offset);
}

void RenderCommand::PushConstants(const CommandBuffer& cmd, const PipelineLayout& pipelineLayout, const void* pushConstants,
    const PushConstantDescription& description)
{
    vkCmdPushConstants(cmd.m_CommandBuffer, pipelineLayout.m_Layout, description.m_StageFlags, 0, description.m_SizeBytes, pushConstants);
}

void RenderCommand::CreateBarrier(const CommandBuffer& cmd, const PipelineBarrierInfo& pipelineBarrierInfo)
{
    VkMemoryBarrier memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = pipelineBarrierInfo.AccessSourceMask;
    memoryBarrier.dstAccessMask = pipelineBarrierInfo.AccessDestinationMask;

    vkCmdPipelineBarrier(cmd.m_CommandBuffer,
        pipelineBarrierInfo.PipelineSourceMask, pipelineBarrierInfo.PipelineDestinationMask,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr);
}

void RenderCommand::CreateBarrier(const CommandBuffer& cmd, const PipelineBufferBarrierInfo& pipelineBarrierInfo)
{
    std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
    bufferMemoryBarriers.reserve(pipelineBarrierInfo.Buffers.size());
    for (auto& buffer : pipelineBarrierInfo.Buffers)
    {
        VkBufferMemoryBarrier bufferMemoryBarrier = {};
        bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferMemoryBarrier.buffer = buffer->m_Buffer;
        bufferMemoryBarrier.size = VK_WHOLE_SIZE;
        bufferMemoryBarrier.srcAccessMask = pipelineBarrierInfo.BufferSourceMask;
        bufferMemoryBarrier.dstAccessMask = pipelineBarrierInfo.BufferDestinationMask;
        bufferMemoryBarrier.srcQueueFamilyIndex = pipelineBarrierInfo.Queue->Family;
        bufferMemoryBarrier.dstQueueFamilyIndex = pipelineBarrierInfo.Queue->Family;

        bufferMemoryBarriers.push_back(bufferMemoryBarrier);
    }

    vkCmdPipelineBarrier(cmd.m_CommandBuffer,
        pipelineBarrierInfo.PipelineSourceMask, pipelineBarrierInfo.PipelineDestinationMask,
        pipelineBarrierInfo.DependencyFlags,
        0, nullptr,
        (u32)bufferMemoryBarriers.size(), bufferMemoryBarriers.data(),
        0, nullptr);
}

void RenderCommand::CreateBarrier(const CommandBuffer& cmd, const PipelineImageBarrierInfo& pipelineBarrierInfo)
{
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.image = pipelineBarrierInfo.Image->m_ImageData.Image;
    imageMemoryBarrier.oldLayout = pipelineBarrierInfo.ImageSourceLayout;
    imageMemoryBarrier.newLayout = pipelineBarrierInfo.ImageDestinationLayout;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.srcAccessMask = pipelineBarrierInfo.ImageSourceMask;
    imageMemoryBarrier.dstAccessMask = pipelineBarrierInfo.ImageDestinationMask;
    imageMemoryBarrier.subresourceRange.aspectMask = pipelineBarrierInfo.ImageAspect;
    imageMemoryBarrier.subresourceRange.baseMipLevel = pipelineBarrierInfo.BaseMipLevel;
    imageMemoryBarrier.subresourceRange.levelCount = pipelineBarrierInfo.MipLevelCount;
    imageMemoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    vkCmdPipelineBarrier(cmd.m_CommandBuffer,
        pipelineBarrierInfo.PipelineSourceMask, pipelineBarrierInfo.PipelineDestinationMask,
        pipelineBarrierInfo.DependencyFlags,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier);
}

void RenderCommand::BeginConditionalRendering(const CommandBuffer& cmd, const Buffer& conditionalBuffer, u64 offset)
{
    VkConditionalRenderingBeginInfoEXT beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
    beginInfo.buffer = conditionalBuffer.m_Buffer;
    beginInfo.offset = offset;

    vkCmdBeginConditionalRenderingEXT(cmd.m_CommandBuffer, &beginInfo);
}

void RenderCommand::EndConditionalRendering(const CommandBuffer& cmd)
{
    vkCmdEndConditionalRenderingEXT(cmd.m_CommandBuffer);
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
