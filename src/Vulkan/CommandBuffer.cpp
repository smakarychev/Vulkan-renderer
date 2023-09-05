#include "CommandBuffer.h"

#include "core.h"
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

    VulkanCheck(vkAllocateCommandBuffers(createInfo.Device, &allocateInfo, &commandBuffer.m_CommandBuffer),
        "Failed to allocate command buffer");

    return commandBuffer;
}

void CommandBuffer::Begin()
{
    VulkanCheck(RenderCommand::ResetCommandBuffer(*this), "Failed to reset command buffer");
    VulkanCheck(RenderCommand::BeginCommandBuffer(*this), "Failed to begin command buffer");
}

void CommandBuffer::End()
{
    VulkanCheck(RenderCommand::EndCommandBuffer(*this), "Failed to end command buffer");
}

void CommandBuffer::Submit(const QueueInfo& queueInfo, const SwapchainFrameSync& frameSync)
{
    VulkanCheck(RenderCommand::SubmitCommandBuffer(*this, queueInfo, frameSync),
        "Failed while submitting command buffer");
}

CommandPool CommandPool::Builder::Build()
{
    CommandPool commandPool = CommandPool::Create(m_CreateInfo);
    Driver::s_DeletionQueue.AddDeleter([commandPool](){ CommandPool::Destroy(commandPool); });

    return commandPool;
}

CommandPool::Builder& CommandPool::Builder::SetQueue(const Device& device, QueueKind queueKind)
{
    Driver::Unpack(device, m_CreateInfo);
    const DeviceQueues& queues = device.GetQueues();
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

    VulkanCheck(vkCreateCommandPool(createInfo.Device, &poolCreateInfo, nullptr, &commandPool.m_CommandPool),
        "Failed to create command pool");
    commandPool.m_Device = createInfo.Device;
    
    return commandPool;
}

void CommandPool::Destroy(const CommandPool& commandPool)
{
    vkDestroyCommandPool(commandPool.m_Device, commandPool.m_CommandPool, nullptr);
}

CommandBuffer CommandPool::AllocateBuffer(CommandBufferKind kind)
{
    CommandBuffer buffer = CommandBuffer::Builder().
        SetPool(*this).
        SetKind(kind).
        Build();

    return buffer;
}