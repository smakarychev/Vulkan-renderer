#include "rendererpch.h"

#include "CommandBuffer.h"

#include "Vulkan/Device.h"

void CommandPool::Reset() const
{
    Device::ResetPool(*this);
}

void CommandBuffer::Reset() const
{
    Device::ResetCommandBuffer(*this);
}

void CommandBuffer::Begin() const
{
    Device::BeginCommandBuffer(*this);
}

void CommandBuffer::Begin(CommandBufferUsage usage) const
{
    Device::BeginCommandBuffer(*this, usage);
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
    m_Pool.Reset();
}

void CommandBufferArray::EnsureCapacity(u32 index)
{
    while (index >= m_Buffers.size())
        m_Buffers.push_back(Device::CreateCommandBuffer({
            .Pool = m_Pool,
            .Kind = CommandBufferKind::Primary}));
}

CommandBufferLabel::CommandBufferLabel(CommandBuffer cmd, std::string_view label)
    : m_Cmd(cmd)
{
    Device::BeginCommandBufferLabel(cmd, label);
}

CommandBufferLabel::~CommandBufferLabel()
{
    Device::EndCommandBufferLabel(m_Cmd);
}
