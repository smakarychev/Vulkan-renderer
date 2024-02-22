#include "Buffer.h"

#include "Vulkan/Driver.h"

Buffer::Builder::Builder(const BufferDescription& description)
{
    SetUsage(description.Usage);
    m_CreateInfo.Description = description;
}

Buffer Buffer::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

Buffer Buffer::Builder::Build(DeletionQueue& deletionQueue)
{
    Buffer buffer = Buffer::Create(m_CreateInfo);
    deletionQueue.Enqueue(buffer);

    return buffer;
}

Buffer Buffer::Builder::BuildManualLifetime()
{
    return Buffer::Create(m_CreateInfo);
}

Buffer::Builder& Buffer::Builder::SetUsage(BufferUsage usage)
{
    m_CreateInfo.Description.Usage |=  usage;

    return *this;
}

Buffer::Builder& Buffer::Builder::SetSizeBytes(u64 sizeBytes)
{
    m_CreateInfo.Description.SizeBytes = sizeBytes;

    return *this;
}

Buffer Buffer::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void Buffer::Destroy(const Buffer& buffer)
{
    Driver::Destroy(buffer.Handle());
}

void Buffer::SetData(const void* data, u64 dataSizeBytes)
{
    Driver::SetBufferData(*this, data, dataSizeBytes, 0);
}

void Buffer::SetData(const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    Driver::SetBufferData(*this, data, dataSizeBytes, offsetBytes);
}

void Buffer::SetData(void* mapped, const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    Driver::SetBufferData(mapped, data, dataSizeBytes, offsetBytes);
}

void* Buffer::Map() const
{
    return Driver::MapBuffer(*this);
}

void Buffer::Unmap() const
{
    Driver::UnmapBuffer(*this);
}

BufferSubresource Buffer::CreateSubresource(u64 sizeBytes, u64 offset) const
{
    ASSERT(offset + sizeBytes < m_Description.SizeBytes, "Invalid subresource range")
    return {
        .Buffer = this,
        .SizeBytes = sizeBytes,
        .Offset = offset};
}
