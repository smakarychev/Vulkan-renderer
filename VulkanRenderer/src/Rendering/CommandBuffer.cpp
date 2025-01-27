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

void CommandBuffer::Submit(QueueKind queueKind, Fence fence) const
{
    Device::SubmitCommandBuffer(*this, queueKind, fence);
}

CommandBufferArray::CommandBufferArray(QueueKind queueKind, bool individualReset)
{
    m_Pool = Device::CreateCommandPool({
        .QueueKind = queueKind,
        .PerBufferReset = individualReset});

    m_Buffers.push_back(Device::CreateCommandBuffer({
        .Pool = m_Pool,
        .Kind = CommandBufferKind::Primary}));
}

void CommandBufferArray::ResetBuffers()
{
    Device::ResetPool(m_Pool);
}

void CommandBufferArray::EnsureCapacity(u32 index)
{
    while (index >= m_Buffers.size())
        m_Buffers.push_back(Device::CreateCommandBuffer({
            .Pool = m_Pool,
            .Kind = CommandBufferKind::Primary}));
}
