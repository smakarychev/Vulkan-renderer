#include "RenderCommand.h"

#include <volk.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include "Device.h"
#include "Rendering/Buffer.h"
#include "Rendering/CommandBuffer.h"
#include "Rendering/Pipeline.h"
#include "Rendering/Swapchain.h"
#include "Rendering/Descriptors.h"

void RenderCommand::BeginRendering(const CommandBuffer& cmd, const RenderingInfo& renderingInfo)
{
    VkRenderingInfo renderingInfoVulkan = {};
    renderingInfoVulkan = {};
    renderingInfoVulkan.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfoVulkan.layerCount = 1;
    renderingInfoVulkan.renderArea = VkRect2D{
        .offset = {},
        .extent = {renderingInfo.m_RenderArea.x, renderingInfo.m_RenderArea.y}};
    renderingInfoVulkan.colorAttachmentCount = (u32)Device::Resources()[renderingInfo].ColorAttachments.size();
    renderingInfoVulkan.pColorAttachments = Device::Resources()[renderingInfo].ColorAttachments.data();
    if (Device::Resources()[renderingInfo].DepthAttachment.has_value())
        renderingInfoVulkan.pDepthAttachment = Device::Resources()[renderingInfo].DepthAttachment.operator->();
    
    vkCmdBeginRendering(Device::Resources()[cmd].CommandBuffer, &renderingInfoVulkan);
}

void RenderCommand::EndRendering(const CommandBuffer& cmd)
{
    vkCmdEndRendering(Device::Resources()[cmd].CommandBuffer);
}

void RenderCommand::ExecuteSecondaryCommandBuffer(const CommandBuffer& cmd, const CommandBuffer& secondary)
{
    vkCmdExecuteCommands(Device::Resources()[cmd].CommandBuffer, 1, &Device::Resources()[secondary].CommandBuffer);
}

void RenderCommand::CopyImage(const CommandBuffer& cmd, const ImageCopyInfo& source, const ImageCopyInfo& destination)
{
    auto&& [copyImageInfo, imageCopy] = Device::CreateVulkanImageCopyInfo(source, destination);
    copyImageInfo.pRegions = &imageCopy;

    vkCmdCopyImage2(Device::Resources()[cmd].CommandBuffer, &copyImageInfo);
}

void RenderCommand::BlitImage(const CommandBuffer& cmd,
    const ImageBlitInfo& source, const ImageBlitInfo& destination, ImageFilter filter)
{
    auto&& [blitImageInfo, imageBlit] = Device::CreateVulkanBlitInfo(source, destination, filter);
    blitImageInfo.pRegions = &imageBlit;
    
    vkCmdBlitImage2(Device::Resources()[cmd].CommandBuffer, &blitImageInfo);
}

void RenderCommand::CopyBuffer(const CommandBuffer& cmd,
    const Buffer& source, const Buffer& destination, const BufferCopyInfo& bufferCopyInfo)
{
    VkBufferCopy2 copy = {};
    copy.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
    copy.size = bufferCopyInfo.SizeBytes;
    copy.srcOffset = bufferCopyInfo.SourceOffset;
    copy.dstOffset = bufferCopyInfo.DestinationOffset;

    const DeviceResources::BufferResource& sourceResource = Device::Resources()[source];
    const DeviceResources::BufferResource& destinationResource = Device::Resources()[destination];
    VkCopyBufferInfo2 copyBufferInfo = {};
    copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
    copyBufferInfo.srcBuffer = sourceResource.Buffer;
    copyBufferInfo.dstBuffer = destinationResource.Buffer;
    copyBufferInfo.regionCount = 1;
    copyBufferInfo.pRegions = &copy;

    vkCmdCopyBuffer2(Device::Resources()[cmd].CommandBuffer, &copyBufferInfo);
}

void RenderCommand::CopyBufferToImage(const CommandBuffer& cmd,
    const Buffer& source, const ImageSubresource& destination)
{
    VkBufferImageCopy2 bufferImageCopy = Device::CreateVulkanImageCopyInfo(destination);

    const DeviceResources::BufferResource& sourceResource = Device::Resources()[source];
    VkCopyBufferToImageInfo2 copyBufferToImageInfo = {};
    copyBufferToImageInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
    copyBufferToImageInfo.srcBuffer = sourceResource.Buffer;
    copyBufferToImageInfo.dstImage = Device::Resources()[*destination.Image].Image;
    copyBufferToImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyBufferToImageInfo.regionCount = 1;
    copyBufferToImageInfo.pRegions = &bufferImageCopy;

    vkCmdCopyBufferToImage2(Device::Resources()[cmd].CommandBuffer, &copyBufferToImageInfo);
}

void RenderCommand::BindVertexBuffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdBindVertexBuffers(Device::Resources()[cmd].CommandBuffer, 0, 1, &Device::Resources()[buffer].Buffer, &offset);
}

void RenderCommand::BindVertexBuffers(const CommandBuffer& cmd, const std::vector<Buffer>& buffers,
    const std::vector<u64>& offsets)
{
    std::vector<VkBuffer> vkBuffers(buffers.size());
    for (u32 i = 0; i < vkBuffers.size(); i++)
        vkBuffers[i] = Device::Resources()[buffers[i]].Buffer;
    
    vkCmdBindVertexBuffers(Device::Resources()[cmd].CommandBuffer, 0, (u32)vkBuffers.size(), vkBuffers.data(),
        offsets.data());
}

void RenderCommand::BindIndexU32Buffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdBindIndexBuffer(Device::Resources()[cmd].CommandBuffer, Device::Resources()[buffer].Buffer, offset,
        VK_INDEX_TYPE_UINT32);
}

void RenderCommand::BindIndexU16Buffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdBindIndexBuffer(Device::Resources()[cmd].CommandBuffer, Device::Resources()[buffer].Buffer, offset,
        VK_INDEX_TYPE_UINT16);
}

void RenderCommand::BindIndexU8Buffer(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdBindIndexBuffer(Device::Resources()[cmd].CommandBuffer, Device::Resources()[buffer].Buffer, offset,
        VK_INDEX_TYPE_UINT8_EXT);
}

void RenderCommand::BindGraphics(const CommandBuffer& cmd, Pipeline pipeline)
{
    vkCmdBindPipeline(Device::Resources()[cmd].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        Device::Resources()[pipeline].Pipeline);
}

void RenderCommand::BindCompute(const CommandBuffer& cmd, Pipeline pipeline)
{
    vkCmdBindPipeline(Device::Resources()[cmd].CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        Device::Resources()[pipeline].Pipeline);
}

void RenderCommand::BindGraphics(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
    PipelineLayout pipelineLayout, u32 setIndex, const std::vector<u32>& dynamicOffsets)
{
    vkCmdBindDescriptorSets(Device::Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, Device::Resources()[pipelineLayout].Layout,
        setIndex, 1,
        &Device::Resources()[descriptorSet].DescriptorSet,
        (u32)dynamicOffsets.size(), dynamicOffsets.data());
}

void RenderCommand::BindCompute(const CommandBuffer& cmd, const DescriptorSet& descriptorSet,
    PipelineLayout pipelineLayout, u32 setIndex, const std::vector<u32>& dynamicOffsets)
{
    vkCmdBindDescriptorSets(Device::Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, Device::Resources()[pipelineLayout].Layout,
        setIndex, 1,
        &Device::Resources()[descriptorSet].DescriptorSet,
        (u32)dynamicOffsets.size(), dynamicOffsets.data());
}

void RenderCommand::BindGraphicsImmutableSamplers(const CommandBuffer& cmd,
    PipelineLayout pipelineLayout, u32 setIndex)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(Device::Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS, Device::Resources()[pipelineLayout].Layout, setIndex);
}

void RenderCommand::BindComputeImmutableSamplers(const CommandBuffer& cmd,
    PipelineLayout pipelineLayout, u32 setIndex)
{
    vkCmdBindDescriptorBufferEmbeddedSamplersEXT(Device::Resources()[cmd].CommandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE, Device::Resources()[pipelineLayout].Layout, setIndex);
}

void RenderCommand::Bind(const CommandBuffer& cmd, const DescriptorArenaAllocator& allocator)
{
    const u64 deviceAddress = Device::GetDeviceAddress(allocator.GetCurrentBuffer());

    VkDescriptorBufferBindingInfoEXT binding = {};
    binding.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    binding.address = deviceAddress;
    binding.usage = allocator.m_Kind == DescriptorAllocatorKind::Resources ?
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT : VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;

    vkCmdBindDescriptorBuffersEXT(Device::Resources()[cmd].CommandBuffer, 1, &binding);
}

void RenderCommand::Bind(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators)
{
    std::vector<VkDescriptorBufferBindingInfoEXT> descriptorBufferBindings;
    descriptorBufferBindings.reserve(allocators.m_Allocators.size());

    for (auto& allocator : allocators.m_Allocators)
    {
        const u64 deviceAddress = Device::GetDeviceAddress(allocator.GetCurrentBuffer());

        VkDescriptorBufferBindingInfoEXT binding = {};
        binding.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        binding.address = deviceAddress;
        binding.usage = allocator.m_Kind == DescriptorAllocatorKind::Resources ?
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT : VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;

        descriptorBufferBindings.push_back(binding);
    }

    vkCmdBindDescriptorBuffersEXT(Device::Resources()[cmd].CommandBuffer, (u32)descriptorBufferBindings.size(),
        descriptorBufferBindings.data());
}

void RenderCommand::BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout, const Descriptors& descriptors, u32 firstSet)
{
    BindDescriptors(cmd, allocators, pipelineLayout, descriptors, firstSet, VK_PIPELINE_BIND_POINT_GRAPHICS);
}

void RenderCommand::BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout, const Descriptors& descriptors, u32 firstSet)
{
    BindDescriptors(cmd, allocators, pipelineLayout, descriptors, firstSet, VK_PIPELINE_BIND_POINT_COMPUTE);
}

void RenderCommand::BindDescriptors(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout, const Descriptors& descriptors, u32 firstSet, VkPipelineBindPoint bindPoint)
{
    ASSERT(&allocators.Get(descriptors.m_Allocator->m_Kind) == descriptors.m_Allocator,
        "Descriptors were not allocated by any of the provided allocators")

    u32 allocatorIndex = (u32)descriptors.m_Allocator->m_Kind;
    u64 offset = descriptors.m_Offsets.front();
    vkCmdSetDescriptorBufferOffsetsEXT(Device::Resources()[cmd].CommandBuffer, bindPoint,
        Device::Resources()[pipelineLayout].Layout, firstSet, 1, &allocatorIndex, &offset);
}

void RenderCommand::Draw(const CommandBuffer& cmd, u32 vertexCount)
{
    vkCmdDraw(Device::Resources()[cmd].CommandBuffer, vertexCount, 1, 0, 0);
}

void RenderCommand::Draw(const CommandBuffer& cmd, u32 vertexCount, u32 baseInstance)
{
    vkCmdDraw(Device::Resources()[cmd].CommandBuffer, vertexCount, 1, 0, baseInstance);
}

void RenderCommand::DrawIndexed(const CommandBuffer& cmd, u32 indexCount)
{
    vkCmdDrawIndexed(Device::Resources()[cmd].CommandBuffer, indexCount, 1, 0, 0, 0);
}

void RenderCommand::DrawIndexed(const CommandBuffer& cmd, u32 indexCount, u32 baseInstance)
{
    vkCmdDrawIndexed(Device::Resources()[cmd].CommandBuffer, indexCount, 1, 0, 0, baseInstance);
}

void RenderCommand::DrawIndexedIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset, u32 count,
    u32 stride)
{
    vkCmdDrawIndexedIndirect(Device::Resources()[cmd].CommandBuffer, Device::Resources()[buffer].Buffer, offset, count,
        stride);    
}

void RenderCommand::DrawIndexedIndirectCount(const CommandBuffer& cmd, const Buffer& drawBuffer, u64 drawOffset,
    const Buffer& countBuffer, u64 countOffset, u32 maxCount, u32 stride)
{
    vkCmdDrawIndexedIndirectCount(Device::Resources()[cmd].CommandBuffer,
        Device::Resources()[drawBuffer].Buffer, drawOffset,
        Device::Resources()[countBuffer].Buffer, countOffset,
        maxCount, stride);
}

void RenderCommand::Dispatch(const CommandBuffer& cmd, const glm::uvec3& groupSize)
{
    vkCmdDispatch(Device::Resources()[cmd].CommandBuffer, groupSize.x, groupSize.y, groupSize.z);
}

void RenderCommand::Dispatch(const CommandBuffer& cmd, const glm::uvec3& invocations, const glm::uvec3& workGroups)
{
    Dispatch(cmd, (invocations + workGroups - glm::uvec3(1)) / workGroups);
}

void RenderCommand::DispatchIndirect(const CommandBuffer& cmd, const Buffer& buffer, u64 offset)
{
    vkCmdDispatchIndirect(Device::Resources()[cmd].CommandBuffer, Device::Resources()[buffer].Buffer, offset);
}

void RenderCommand::PushConstants(const CommandBuffer& cmd, PipelineLayout pipelineLayout, const void* pushConstants)
{
    VkPushConstantRange& pushConstantRange = Device::Resources()[pipelineLayout].PushConstants.front();
    vkCmdPushConstants(Device::Resources()[cmd].CommandBuffer, Device::Resources()[pipelineLayout].Layout,
        pushConstantRange.stageFlags, 0, pushConstantRange.size, pushConstants);
}

void RenderCommand::WaitOnFullPipelineBarrier(const CommandBuffer& cmd)
{
    VkMemoryBarrier2 memoryBarrier = {};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &memoryBarrier;

    vkCmdPipelineBarrier2(Device::Resources()[cmd].CommandBuffer, &dependencyInfo);
}

void RenderCommand::WaitOnBarrier(const CommandBuffer& cmd, DependencyInfo dependencyInfo)
{
    VkDependencyInfo vkDependencyInfo = Device::Resources()[dependencyInfo].DependencyInfo;
    vkDependencyInfo.memoryBarrierCount =
        (u32)Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.size();
    vkDependencyInfo.pMemoryBarriers = Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.data();
    vkDependencyInfo.imageMemoryBarrierCount = (u32)Device::Resources()[dependencyInfo].LayoutTransitionsInfo.size();
    vkDependencyInfo.pImageMemoryBarriers = Device::Resources()[dependencyInfo].LayoutTransitionsInfo.data();
    vkCmdPipelineBarrier2(Device::Resources()[cmd].CommandBuffer, &vkDependencyInfo);
}

void RenderCommand::SignalSplitBarrier(const CommandBuffer& cmd, SplitBarrier splitBarrier,
    DependencyInfo dependencyInfo)
{
    VkDependencyInfo vkDependencyInfo = Device::Resources()[dependencyInfo].DependencyInfo;
    vkDependencyInfo.memoryBarrierCount =
        (u32)Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.size();
    vkDependencyInfo.pMemoryBarriers = Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.data();
    vkDependencyInfo.imageMemoryBarrierCount = (u32)Device::Resources()[dependencyInfo].LayoutTransitionsInfo.size();
    vkDependencyInfo.pImageMemoryBarriers = Device::Resources()[dependencyInfo].LayoutTransitionsInfo.data();
    vkCmdSetEvent2(Device::Resources()[cmd].CommandBuffer, Device::Resources()[splitBarrier].Event, &vkDependencyInfo);
}

void RenderCommand::WaitOnSplitBarrier(const CommandBuffer& cmd, SplitBarrier splitBarrier,
    DependencyInfo dependencyInfo)
{
    VkDependencyInfo vkDependencyInfo = Device::Resources()[dependencyInfo].DependencyInfo;
    vkDependencyInfo.memoryBarrierCount =
        (u32)Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.size();
    vkDependencyInfo.pMemoryBarriers = Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.data();
    vkDependencyInfo.imageMemoryBarrierCount = (u32)Device::Resources()[dependencyInfo].LayoutTransitionsInfo.size();
    vkDependencyInfo.pImageMemoryBarriers = Device::Resources()[dependencyInfo].LayoutTransitionsInfo.data();
    vkCmdWaitEvents2(Device::Resources()[cmd].CommandBuffer, 1, &Device::Resources()[splitBarrier].Event,
        &vkDependencyInfo);
}

void RenderCommand::ResetSplitBarrier(const CommandBuffer& cmd, SplitBarrier splitBarrier,
    DependencyInfo dependencyInfo)
{
    ASSERT(!Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.empty(), "Invalid reset operation")
    vkCmdResetEvent2(Device::Resources()[cmd].CommandBuffer, Device::Resources()[splitBarrier].Event,
        Device::Resources()[dependencyInfo].ExecutionMemoryDependenciesInfo.front().dstStageMask);
}

void RenderCommand::BeginConditionalRendering(const CommandBuffer& cmd, const Buffer& conditionalBuffer, u64 offset)
{
    VkConditionalRenderingBeginInfoEXT beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
    beginInfo.buffer = Device::Resources()[conditionalBuffer].Buffer;
    beginInfo.offset = offset;

    vkCmdBeginConditionalRenderingEXT(Device::Resources()[cmd].CommandBuffer, &beginInfo);
}

void RenderCommand::EndConditionalRendering(const CommandBuffer& cmd)
{
    vkCmdEndConditionalRenderingEXT(Device::Resources()[cmd].CommandBuffer);
}

void RenderCommand::SetViewport(const CommandBuffer& cmd, const glm::vec2& size)
{
    VkViewport viewport = {
        .x = 0, .y = 0,
        .width = (f32)size.x, .height = (f32)size.y,
        .minDepth = 0.0f, .maxDepth = 1.0f};

    vkCmdSetViewport(Device::Resources()[cmd].CommandBuffer, 0, 1, &viewport);    
}

void RenderCommand::SetScissors(const CommandBuffer& cmd, const glm::vec2& offset, const glm::vec2& size)
{
    VkRect2D scissor = {
        .offset = {(i32)offset.x, (i32)offset.y},
        .extent = {(u32)size.x, (u32)size.y}};
    
    vkCmdSetScissor(Device::Resources()[cmd].CommandBuffer, 0, 1, &scissor);
}

void RenderCommand::SetDepthBias(const CommandBuffer& cmd, const DepthBias& depthBias)
{
    vkCmdSetDepthBias(Device::Resources()[cmd].CommandBuffer, depthBias.Constant, 0.0f, depthBias.Slope);
}

void RenderCommand::ImGuiBeginFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void RenderCommand::DrawImGui(const CommandBuffer& cmd, const RenderingInfo& renderingInfo)
{
    ImGui::Render();
    BeginRendering(cmd, renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Device::Resources()[cmd].CommandBuffer);
    EndRendering(cmd);
}
