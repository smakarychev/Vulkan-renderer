#include "Buffer.h"

#include "Vulkan/Device.h"

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

void Buffer::SetData(Span<const std::byte> data)
{
    ASSERT(data.size() <= m_Description.SizeBytes, "Attempt to write data outside of buffer region")
    Device::SetBufferData(*this, data, 0);
}

void Buffer::SetData(Span<const std::byte> data, u64 offsetBytes)
{
    ASSERT(data.size() + offsetBytes <= m_Description.SizeBytes, "Attempt to write data outside of buffer region")
    Device::SetBufferData(*this, data, offsetBytes);
}

void Buffer::SetData(void* mapped, Span<const std::byte> data, u64 offsetBytes)
{
    ASSERT((u64)((const u8*)mapped + data.size() + offsetBytes - (const u8*)m_HostAddress) <= m_Description.SizeBytes,
        "Attempt to write data outside of buffer region")
    Device::SetBufferData(mapped, data, offsetBytes);
}

void* Buffer::Map()
{
    m_HostAddress = Device::MapBuffer(*this);
    
    return m_HostAddress;
}

void Buffer::Unmap()
{
    m_HostAddress = nullptr;
    
    Device::UnmapBuffer(*this);
}
