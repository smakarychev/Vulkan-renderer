#include "rendererpch.h"

#include "Buffer.h"

#include "Vulkan/Device.h"

void Buffer::Resize(u64 newSize, CommandBuffer cmd, bool copyData) const
{
    Device::ResizeBuffer(*this, newSize, cmd, copyData);
}

void* Buffer::Map() const
{
    return Device::MapBuffer(*this);
}

void Buffer::Unmap() const
{
    Device::UnmapBuffer(*this);
}

void Buffer::SetData(Span<const std::byte> data, u64 offsetBytes) const
{
    Device::SetBufferData(*this, data, offsetBytes);
}

void Buffer::SetData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes)
{
    Device::SetBufferData(mappedAddress, data, offsetBytes);
}

void* Buffer::GetMappedAddress() const
{
    return Device::GetBufferMappedAddress(*this);
}

usize Buffer::GetSizeBytes() const
{
    return Device::GetBufferSizeBytes(*this);
}

const BufferDescription& Buffer::GetDescription() const
{
    return Device::GetBufferDescription(*this);
}

u64 Buffer::GetDeviceAddress() const
{
    return Device::GetDeviceAddress(*this);
}
