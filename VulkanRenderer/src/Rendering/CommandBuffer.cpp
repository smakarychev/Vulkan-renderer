#include "CommandBuffer.h"

#include "Vulkan/Device.h"

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
