#include "Buffer.h"

#include "Vulkan/Driver.h"

std::string BufferUtils::bufferUsageToString(BufferUsage usage)
{
    std::string usageString = "";
    
    if (enumHasAny(usage, BufferUsage::Vertex))
        usageString += usageString.empty() ? "Vertex" : " | Vertex";
    if (enumHasAny(usage, BufferUsage::Index))
        usageString += usageString.empty() ? "Index" : " | Index";
    if (enumHasAny(usage, BufferUsage::Uniform))
        usageString += usageString.empty() ? "Uniform" : " | Uniform";
    if (enumHasAny(usage, BufferUsage::Storage))
        usageString += usageString.empty() ? "Storage" : " | Storage";
    if (enumHasAny(usage, BufferUsage::Indirect))
        usageString += usageString.empty() ? "Indirect" : " | Indirect";
    if (enumHasAny(usage, BufferUsage::Upload))
        usageString += usageString.empty() ? "Upload" : " | Upload";
    if (enumHasAny(usage, BufferUsage::UploadRandomAccess))
        usageString += usageString.empty() ? "UploadRandomAccess" : " | UploadRandomAccess";
    if (enumHasAny(usage, BufferUsage::Readback))
        usageString += usageString.empty() ? "Readback" : " | Readback";
    if (enumHasAny(usage, BufferUsage::Source))
        usageString += usageString.empty() ? "Source" : " | Source";
    if (enumHasAny(usage, BufferUsage::Destination))
        usageString += usageString.empty() ? "Destination" : " | Destination";
    if (enumHasAny(usage, BufferUsage::Conditional))
        usageString += usageString.empty() ? "Conditional" : " | Conditional";
    if (enumHasAny(usage, BufferUsage::DeviceAddress))
        usageString += usageString.empty() ? "DeviceAddress" : " | DeviceAddress";

    return usageString;
}

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

Buffer::Builder& Buffer::Builder::CreateMapped()
{
    m_CreateInfo.CreateMapped = true;

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

void* Buffer::Map()
{
    m_HostAddress = Driver::MapBuffer(*this);
    
    return m_HostAddress;
}

void Buffer::Unmap()
{
    m_HostAddress = nullptr;
    
    Driver::UnmapBuffer(*this);
}

BufferSubresource Buffer::CreateSubresource() const
{
    return CreateSubresource(m_Description.SizeBytes, 0);
}

BufferSubresource Buffer::CreateSubresource(u64 sizeBytes, u64 offset) const
{
    ASSERT(offset + sizeBytes <= m_Description.SizeBytes, "Invalid subresource range")
    return {
        .Buffer = this,
        .SizeBytes = sizeBytes,
        .Offset = offset};
}
