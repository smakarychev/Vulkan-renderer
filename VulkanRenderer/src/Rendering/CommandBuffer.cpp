#include "CommandBuffer.h"

#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"

void CommandBuffer::Reset() const
{
    Device::ResetCommandBuffer(*this);
}

void CommandBuffer::Begin() const
{
    Begin(CommandBufferUsage::SingleSubmit);
}

void CommandBuffer::Begin(CommandBufferUsage commandBufferUsage) const
{
    Device::BeginCommandBuffer(*this, commandBufferUsage);
}

void CommandBuffer::End() const
{
    Device::EndCommandBuffer(*this);
}

void CommandBuffer::Submit(QueueKind queueKind, const BufferSubmitSyncInfo& submitSync) const
{
   Device::SubmitCommandBuffer(*this, queueKind, submitSync);
}

void CommandBuffer::Submit(QueueKind queueKind, const BufferSubmitTimelineSyncInfo& submitSync) const
{
    Device::SubmitCommandBuffer(*this, queueKind, submitSync);
}

void CommandBuffer::Submit(QueueKind queueKind, const Fence& fence) const
{
    Device::SubmitCommandBuffer(*this, queueKind, fence);
}

void CommandBuffer::Submit(QueueKind queueKind, const Fence* fence) const
{
    Device::SubmitCommandBuffer(*this, queueKind, fence);
}



CommandPool CommandPool::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

CommandPool CommandPool::Builder::Build(DeletionQueue& deletionQueue)
{
    CommandPool commandPool = CommandPool::Create(m_CreateInfo);
    deletionQueue.Enqueue(commandPool);

    return commandPool;
}

CommandPool CommandPool::Builder::BuildManualLifetime()
{
    return CommandPool::Create(m_CreateInfo);
}

CommandPool::Builder& CommandPool::Builder::SetQueue(QueueKind queueKind)
{
    m_CreateInfo.QueueKind = queueKind;
    
    return *this;
}

CommandPool::Builder& CommandPool::Builder::PerBufferReset(bool enabled)
{
    m_CreateInfo.PerBufferReset = enabled;
    
    return *this;
}

CommandPool CommandPool::Create(const Builder::CreateInfo& createInfo)
{
    return Device::Create(createInfo);
}

void CommandPool::Destroy(const CommandPool& commandPool)
{
    return Device::Destroy(commandPool.Handle());
}

CommandBuffer CommandPool::AllocateBuffer(CommandBufferKind kind)
{
    CommandBuffer buffer = Device::CreateCommandBuffer({
        .Pool = this,
        .Kind = kind});

    return buffer;
}

void CommandPool::Reset() const
{
    Device::ResetPool(*this);
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
