﻿#include "Buffer.h"

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
    if (enumHasAny(usage, BufferUsage::Mappable))
        usageString += usageString.empty() ? "Mappable" : " | Mappable";
    if (enumHasAny(usage, BufferUsage::MappableRandomAccess))
        usageString += usageString.empty() ? "MappableRandomAccess" : " | MappableRandomAccess";
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

Buffer::Builder& Buffer::Builder::CreateMapped()
{
    m_CreateInfo.Description.Usage |= BufferUsage::Mappable;
    m_CreateInfo.CreateMapped = true;

    return *this;
}

Buffer::Builder& Buffer::Builder::CreateMappedRandomAccess()
{
    m_CreateInfo.Description.Usage |= BufferUsage::MappableRandomAccess;
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
    ASSERT(dataSizeBytes <= m_Description.SizeBytes,
        "Attempt to write data outside of buffer region")
    Driver::SetBufferData(*this, data, dataSizeBytes, 0);
}

void Buffer::SetData(const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    ASSERT(dataSizeBytes + offsetBytes <= m_Description.SizeBytes,
        "Attempt to write data outside of buffer region")
    Driver::SetBufferData(*this, data, dataSizeBytes, offsetBytes);
}

void Buffer::SetData(void* mapped, const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    ASSERT((u64)((const u8*)mapped + dataSizeBytes + offsetBytes - (const u8*)m_HostAddress) <= m_Description.SizeBytes,
        "Attempt to write data outside of buffer region")
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

BufferSubresource Buffer::Subresource() const
{
    return Subresource(m_Description.SizeBytes, 0);
}

BufferSubresource Buffer::Subresource(u64 sizeBytes, u64 offset) const
{
    return Subresource({
        .SizeBytes = sizeBytes,
        .Offset = offset});
}

BufferSubresource Buffer::Subresource(const BufferSubresourceDescription& description) const
{
    ASSERT(description.Offset + description.SizeBytes <= m_Description.SizeBytes, "Invalid subresource range")

    return BufferSubresource{
        .Buffer = this,
        .Description = description};
}
