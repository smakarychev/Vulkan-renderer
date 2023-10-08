#include "CommandBuffer.h"

#include "Core/core.h"
#include "Driver.h"
#include "RenderCommand.h"

namespace
{
    VkCommandBufferLevel vkBufferLevelByKind(CommandBufferKind kind)
    {
        switch (kind) {
        case CommandBufferKind::Primary:    return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        case CommandBufferKind::Secondary:  return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        default:
            ASSERT(false, "Unrecognized command buffer kind")
            break;
        }
        std::unreachable();
    }
}

CommandBuffer CommandBuffer::Builder::Build()
{
    return CommandBuffer::Create(m_CreateInfo);
}

CommandBuffer::Builder& CommandBuffer::Builder::SetPool(const CommandPool& pool)
{
    Driver::Unpack(pool, m_CreateInfo);

    return *this;
}

CommandBuffer::Builder& CommandBuffer::Builder::SetKind(CommandBufferKind kind)
{
    m_CreateInfo.Level = vkBufferLevelByKind(kind);

    return *this;
}

CommandBuffer CommandBuffer::Create(const Builder::CreateInfo& createInfo)
{
    CommandBuffer commandBuffer = {};
    
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = createInfo.CommandPool;
    allocateInfo.level = createInfo.Level;
    allocateInfo.commandBufferCount = 1;

    VulkanCheck(vkAllocateCommandBuffers(Driver::DeviceHandle(), &allocateInfo, &commandBuffer.m_CommandBuffer),
        "Failed to allocate command buffer");

    return commandBuffer;
}

void CommandBuffer::Reset() const
{
    VulkanCheck(RenderCommand::ResetCommandBuffer(*this), "Failed to reset command buffer");
}

void CommandBuffer::Begin() const
{
    VulkanCheck(RenderCommand::BeginCommandBuffer(*this), "Failed to begin command buffer");
}

void CommandBuffer::End() const
{
    VulkanCheck(RenderCommand::EndCommandBuffer(*this), "Failed to end command buffer");
}

void CommandBuffer::Submit(const QueueInfo& queueInfo, const BufferSubmitSyncInfo& submitSync) const
{
    VulkanCheck(RenderCommand::SubmitCommandBuffer(*this, queueInfo, submitSync),
        "Failed while submitting command buffer");
}

void CommandBuffer::Submit(const QueueInfo& queueInfo, const Fence& fence) const
{
    VulkanCheck(RenderCommand::SubmitCommandBuffer(*this, queueInfo, fence),
        "Failed while submitting command buffer");
}

CommandPool CommandPool::Builder::Build()
{
    CommandPool commandPool = CommandPool::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([commandPool](){ CommandPool::Destroy(commandPool); });

    return commandPool;
}

CommandPool CommandPool::Builder::BuildManualLifetime()
{
    return CommandPool::Create(m_CreateInfo);
}

CommandPool::Builder& CommandPool::Builder::SetQueue(QueueKind queueKind)
{
    const DeviceQueues& queues = Driver::GetDevice().GetQueues();
    m_CreateInfo.QueueFamily = queues.GetFamilyByKind(queueKind);
    
    return *this;
}

CommandPool::Builder& CommandPool::Builder::PerBufferReset(bool enabled)
{
    if (enabled)
        m_CreateInfo.Flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    else
        m_CreateInfo.Flags &= ~VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    return *this;
}

CommandPool CommandPool::Create(const Builder::CreateInfo& createInfo)
{
    CommandPool commandPool = {};
    
    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = createInfo.Flags;
    poolCreateInfo.queueFamilyIndex = createInfo.QueueFamily;

    VulkanCheck(vkCreateCommandPool(Driver::DeviceHandle(), &poolCreateInfo, nullptr, &commandPool.m_CommandPool),
        "Failed to create command pool");
    
    return commandPool;
}

void CommandPool::Destroy(const CommandPool& commandPool)
{
    vkDestroyCommandPool(Driver::DeviceHandle(), commandPool.m_CommandPool, nullptr);
}

CommandBuffer CommandPool::AllocateBuffer(CommandBufferKind kind)
{
    CommandBuffer buffer = CommandBuffer::Builder()
        .SetPool(*this)
        .SetKind(kind)
        .Build();

    return buffer;
}

void CommandPool::Reset() const
{
    VulkanCheck(RenderCommand::ResetPool(*this), "Failed to reset command pool");
}
