#include "rendererpch.h"

#include "BufferArena.h"

#include "Vulkan/Device.h"

void BufferArena::ResizePhysical(u64 newSize, CommandBuffer cmd, bool copyData) const
{
    Device::ResizeBufferArenaPhysical(*this, newSize, cmd, copyData);
}

Buffer BufferArena::GetUnderlyingBuffer() const
{
    return Device::GetBufferArenaUnderlyingBuffer(*this);
}

u64 BufferArena::GetSizeBytesPhysical() const
{
    return Device::GetBufferArenaSizeBytesPhysical(*this);
}

BufferSuballocationResult BufferArena::Suballocate(u64 sizeBytes, u32 alignment) const
{
    return Device::BufferArenaSuballocate(*this, sizeBytes, alignment);
}

void BufferArena::Free(BufferSuballocationHandle suballocation) const
{
    Device::BufferArenaFree(*this, suballocation);
}
