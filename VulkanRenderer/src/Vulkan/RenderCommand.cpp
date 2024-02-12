#include "RenderCommand.h"

#include <algorithm>
#include <ranges>
#include <volk.h>

#include "Buffer.h"
#include "CommandBuffer.h"
#include "Driver.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Synchronization.h"

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
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = {};
    commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSubmitInfo.commandBuffer = cmd.m_CommandBuffer;

    VkSubmitInfo2 submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
    
    return vkQueueSubmit2(queueInfo.Queue, 1, &submitInfo, fence ? fence->m_Fence : VK_NULL_HANDLE);
}

VkResult RenderCommand::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo,
    const BufferSubmitSyncInfo& submitSync)
{
    std::vector<VkCommandBufferSubmitInfo> commandBufferSubmitInfos;
    commandBufferSubmitInfos.reserve(cmds.size());
    for (auto& cmd : cmds)
        commandBufferSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd.m_CommandBuffer});

    std::vector<VkSemaphoreSubmitInfo> signalSemaphoreSubmitInfos;
    signalSemaphoreSubmitInfos.reserve(submitSync.SignalSemaphores.size());
    for (auto& semaphore : submitSync.SignalSemaphores)
        signalSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = semaphore->m_Semaphore});

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos;
    waitSemaphoreSubmitInfos.reserve(submitSync.WaitSemaphores.size());
    for (u32 i = 0; i < submitSync.WaitSemaphores.size(); i++)
        waitSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = submitSync.WaitSemaphores[i]->m_Semaphore,
            .stageMask = submitSync.WaitStages[i]});
    
    VkSubmitInfo2  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    return vkQueueSubmit2(queueInfo.Queue, 1, &submitInfo, submitSync.Fence ?
        submitSync.Fence->m_Fence : VK_NULL_HANDLE);
}

VkResult RenderCommand::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo,
    const BufferSubmitTimelineSyncInfo& submitSync)
{
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        submitSync.SignalSemaphores[i]->SetTimeline(submitSync.SignalValues[i]);

    std::vector<VkCommandBufferSubmitInfo> commandBufferSubmitInfos;
    commandBufferSubmitInfos.reserve(cmds.size());
    for (auto& cmd : cmds)
        commandBufferSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd.m_CommandBuffer});

    std::vector<VkSemaphoreSubmitInfo> signalSemaphoreSubmitInfos;
    signalSemaphoreSubmitInfos.reserve(submitSync.SignalSemaphores.size());
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        signalSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = submitSync.SignalSemaphores[i]->m_Semaphore,
            .value = submitSync.SignalValues[i]});

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos;
    waitSemaphoreSubmitInfos.reserve(submitSync.WaitSemaphores.size());
    for (u32 i = 0; i < submitSync.WaitSemaphores.size(); i++)
        waitSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = submitSync.WaitSemaphores[i]->m_Semaphore,
            .value = submitSync.WaitValues[i],
            .stageMask = submitSync.WaitStages[i]});
    
    VkSubmitInfo2  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    return vkQueueSubmit2(queueInfo.Queue, 1, &submitInfo, submitSync.Fence ?
        submitSync.Fence->m_Fence : VK_NULL_HANDLE);
}

VkResult RenderCommand::SubmitCommandBuffers(const std::vector<CommandBuffer>& cmds, const QueueInfo& queueInfo,
    const BufferSubmitMixedSyncInfo& submitSync)
{
    for (u32 i = 0; i < submitSync.SignalTimelineSemaphores.size(); i++)
        submitSync.SignalTimelineSemaphores[i]->SetTimeline(submitSync.SignalValues[i]);

    std::vector<VkCommandBufferSubmitInfo> commandBufferSubmitInfos;
    commandBufferSubmitInfos.reserve(cmds.size());
    for (auto& cmd : cmds)
        commandBufferSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd.m_CommandBuffer});

    std::vector<VkSemaphoreSubmitInfo> signalSemaphoreSubmitInfos;
    signalSemaphoreSubmitInfos.reserve(submitSync.SignalSemaphores.size());
    for (u32 i = 0; i < submitSync.SignalSemaphores.size(); i++)
        signalSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = submitSync.SignalSemaphores[i]->m_Semaphore,
            .value = i < submitSync.SignalSemaphores.size() ?
                0 : submitSync.SignalValues[i - submitSync.SignalSemaphores.size()]});

    std::vector<VkSemaphoreSubmitInfo> waitSemaphoreSubmitInfos;
    waitSemaphoreSubmitInfos.reserve(submitSync.WaitSemaphores.size());
    for (u32 i = 0; i < submitSync.WaitSemaphores.size(); i++)
        waitSemaphoreSubmitInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = submitSync.WaitSemaphores[i]->m_Semaphore,
            .value = i < submitSync.WaitSemaphores.size() ?
                0 : submitSync.WaitValues[i - submitSync.WaitSemaphores.size()],
            .stageMask = submitSync.WaitStages[i]});
    
    VkSubmitInfo2  submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = (u32)commandBufferSubmitInfos.size();
    submitInfo.pCommandBufferInfos = commandBufferSubmitInfos.data();
    submitInfo.signalSemaphoreInfoCount = (u32)signalSemaphoreSubmitInfos.size();
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreSubmitInfos.data();
    submitInfo.waitSemaphoreInfoCount = (u32)waitSemaphoreSubmitInfos.size();
    submitInfo.pWaitSemaphoreInfos = waitSemaphoreSubmitInfos.data();

    return vkQueueSubmit2(queueInfo.Queue, 1, &submitInfo, submitSync.Fence ?
        submitSync.Fence->m_Fence : VK_NULL_HANDLE);
}

void RenderCommand::ExecuteSecondaryCommandBuffer(const CommandBuffer& cmd, const CommandBuffer& secondary)
{
    vkCmdExecuteCommands(cmd.m_CommandBuffer, 1, &secondary.m_CommandBuffer);
}

void RenderCommand::BlitImage(const CommandBuffer& cmd,
    const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter)
{
    auto&& [blitImageInfo, imageBlit] = Image::CreateVulkanBlitInfo(source, destination, filter);
    blitImageInfo.pRegions = &imageBlit;
    
    vkCmdBlitImage2(cmd.m_CommandBuffer, &blitImageInfo);
}

void RenderCommand::CopyBuffer(const CommandBuffer& cmd,
    const Buffer& source, const Buffer& destination, const BufferCopyInfo& bufferCopyInfo)
{
    VkBufferCopy2 copy = {};
    copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
    copy.size = bufferCopyInfo.SizeBytes;
    copy.srcOffset = bufferCopyInfo.SourceOffset;
    copy.dstOffset = bufferCopyInfo.DestinationOffset;

    VkCopyBufferInfo2 copyBufferInfo = {};
    copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
    copyBufferInfo.srcBuffer = source.m_Buffer;
    copyBufferInfo.dstBuffer = destination.m_Buffer;
    copyBufferInfo.regionCount = 1;
    copyBufferInfo.pRegions = &copy;

    vkCmdCopyBuffer2(cmd.m_CommandBuffer, &copyBufferInfo);
}

void RenderCommand::CopyBufferToImage(const CommandBuffer& cmd,
    const Buffer& source, const ImageSubresource& destination)
{
    VkBufferImageCopy2 bufferImageCopy = Image::CreateVulkanImageCopyInfo(destination);

    VkCopyBufferToImageInfo2 copyBufferToImageInfo = {};
    copyBufferToImageInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
    copyBufferToImageInfo.srcBuffer = source.m_Buffer;
    copyBufferToImageInfo.dstImage = destination.Image->m_Image;
    copyBufferToImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyBufferToImageInfo.regionCount = 1;
    copyBufferToImageInfo.pRegions = &bufferImageCopy;

    vkCmdCopyBufferToImage2(cmd.m_CommandBuffer, &copyBufferToImageInfo);
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
    //vkCmdBindIndexBuffer(cmd.m_CommandBuffer, buffer.m_Buffer, offset, VK_INDEX_TYPE_UINT8_EXT);
    vkCmdBindIndexBuffer(cmd.m_CommandBuffer, buffer.m_Buffer, offset, VK_INDEX_TYPE_UINT32);
}

void RenderCommand::BindPipeline(const CommandBuffer& cmd, const Pipeline& pipeline, VkPipelineBindPoint bindPoint)
{
    vkCmdBindPipeline(cmd.m_CommandBuffer, bindPoint, pipeline.m_Pipeline);
}

void RenderCommand::BindDescriptorSet(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
    const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint,
    const std::vector<u32>& dynamicOffsets)
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

void RenderCommand::PushConstants(const CommandBuffer& cmd, const PipelineLayout& pipelineLayout,
    const void* pushConstants, const PushConstantDescription& description)
{
    vkCmdPushConstants(cmd.m_CommandBuffer, pipelineLayout.m_Layout, description.m_StageFlags, 0,
        description.m_SizeBytes, pushConstants);
}

void RenderCommand::WaitOnBarrier(const CommandBuffer& cmd, const DependencyInfo& dependencyInfo)
{
    VkDependencyInfo vkDependencyInfo = dependencyInfo.m_DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = (u32)dependencyInfo.m_ExecutionMemoryDependenciesInfo.size();
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.m_ExecutionMemoryDependenciesInfo.data();
    vkDependencyInfo.imageMemoryBarrierCount = (u32)dependencyInfo.m_LayoutTransitionInfo.size();
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.m_LayoutTransitionInfo.data();
    vkCmdPipelineBarrier2(cmd.m_CommandBuffer, &vkDependencyInfo);
}

void RenderCommand::SignalSplitBarrier(const CommandBuffer& cmd, const SplitBarrier& splitBarrier,
    const DependencyInfo& dependencyInfo)
{
    VkDependencyInfo vkDependencyInfo = dependencyInfo.m_DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = (u32)dependencyInfo.m_ExecutionMemoryDependenciesInfo.size();
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.m_ExecutionMemoryDependenciesInfo.data();
    vkDependencyInfo.imageMemoryBarrierCount = (u32)dependencyInfo.m_LayoutTransitionInfo.size();
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.m_LayoutTransitionInfo.data();
    vkCmdSetEvent2(cmd.m_CommandBuffer, splitBarrier.m_Event, &vkDependencyInfo);
}

void RenderCommand::WaitOnSplitBarrier(const CommandBuffer& cmd, const SplitBarrier& splitBarrier,
    const DependencyInfo& dependencyInfo)
{
    VkDependencyInfo vkDependencyInfo = dependencyInfo.m_DependencyInfo;
    vkDependencyInfo.memoryBarrierCount = (u32)dependencyInfo.m_ExecutionMemoryDependenciesInfo.size();
    vkDependencyInfo.pMemoryBarriers = dependencyInfo.m_ExecutionMemoryDependenciesInfo.data();
    vkDependencyInfo.imageMemoryBarrierCount = (u32)dependencyInfo.m_LayoutTransitionInfo.size();
    vkDependencyInfo.pImageMemoryBarriers = dependencyInfo.m_LayoutTransitionInfo.data();
    vkCmdWaitEvents2(cmd.m_CommandBuffer, 1, &splitBarrier.m_Event, &vkDependencyInfo);
}

void RenderCommand::ResetSplitBarrier(const CommandBuffer& cmd, const SplitBarrier& splitBarrier,
    const DependencyInfo& dependencyInfo)
{
    ASSERT(!dependencyInfo.m_ExecutionMemoryDependenciesInfo.empty(), "Invalid reset operation")
    vkCmdResetEvent2(cmd.m_CommandBuffer, splitBarrier.m_Event,
        dependencyInfo.m_ExecutionMemoryDependenciesInfo.front().dstStageMask);
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
