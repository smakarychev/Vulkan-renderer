#include "CommandBuffer.h"

#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

CommandBuffer CommandBuffer::Builder::Build()
{
    return CommandBuffer::Create(m_CreateInfo);
}

CommandBuffer::Builder& CommandBuffer::Builder::SetPool(const CommandPool& pool)
{
    m_CreateInfo.Pool = &pool;

    return *this;
}

CommandBuffer::Builder& CommandBuffer::Builder::SetKind(CommandBufferKind kind)
{
    m_CreateInfo.Kind = kind;

    return *this;
}

CommandBuffer CommandBuffer::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void CommandBuffer::Reset() const
{
    RenderCommand::ResetCommandBuffer(*this);
}

void CommandBuffer::Begin() const
{
    Begin(CommandBufferUsage::SingleSubmit);
}

void CommandBuffer::Begin(CommandBufferUsage commandBufferUsage) const
{
    RenderCommand::BeginCommandBuffer(*this, commandBufferUsage);
}

void CommandBuffer::End() const
{
    RenderCommand::EndCommandBuffer(*this);
}

void CommandBuffer::Submit(const QueueInfo& queueInfo, const BufferSubmitSyncInfo& submitSync) const
{
   RenderCommand::SubmitCommandBuffer(*this, queueInfo, submitSync);
}

void CommandBuffer::Submit(const QueueInfo& queueInfo, const BufferSubmitTimelineSyncInfo& submitSync) const
{
    RenderCommand::SubmitCommandBuffer(*this, queueInfo, submitSync);
}

void CommandBuffer::Submit(const QueueInfo& queueInfo, const Fence& fence) const
{
    RenderCommand::SubmitCommandBuffer(*this, queueInfo, fence);
}

void CommandBuffer::Submit(const QueueInfo& queueInfo, const Fence* fence) const
{
    RenderCommand::SubmitCommandBuffer(*this, queueInfo, fence);
}



CommandPool CommandPool::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

CommandPool CommandPool::Builder::Build(DeletionQueue& deletionQueue)
{
    CommandPool commandPool = CommandPool::Create(m_CreateInfo);
    deletionQueue.AddDeleter([commandPool](){ CommandPool::Destroy(commandPool); });

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
    m_CreateInfo.PerBufferReset = enabled;
    
    return *this;
}

CommandPool CommandPool::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void CommandPool::Destroy(const CommandPool& commandPool)
{
    return Driver::Destroy(commandPool);
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
    RenderCommand::ResetPool(*this);
}



CommandBufferArray::CommandBufferArray(QueueKind queueKind, bool individualReset)
{
    m_Pool = CommandPool::Builder()
        .SetQueue(queueKind)
        .PerBufferReset(individualReset)
        .Build();

    m_Buffers.push_back(m_Pool.AllocateBuffer(CommandBufferKind::Primary));
}

void CommandBufferArray::EnsureCapacity(u32 index)
{
    while (index >= m_Buffers.size())
        m_Buffers.push_back(m_Pool.AllocateBuffer(CommandBufferKind::Primary));
}
