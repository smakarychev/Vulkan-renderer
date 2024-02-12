#include "Buffer.h"

#include "Driver.h"
#include "VulkanCore.h"
#include "VulkanUtils.h"

namespace 
{
    VkBufferUsageFlags vulkanBufferUsageFromUsage(BufferUsage kind)
    {
        ASSERT(!enumHasAll(kind, BufferUsage::Vertex | BufferUsage::Index),
            "Buffer usage cannot include both vertex and index")
        
        VkBufferUsageFlags flags = 0;
        if (enumHasAny(kind, BufferUsage::Vertex))
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (enumHasAny(kind, BufferUsage::Index))
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (enumHasAny(kind, BufferUsage::Uniform))
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (enumHasAny(kind, BufferUsage::Storage))
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (enumHasAny(kind, BufferUsage::Indirect))
            flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        if (enumHasAny(kind, BufferUsage::Source))
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (enumHasAny(kind, BufferUsage::Destination))
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (enumHasAny(kind, BufferUsage::Conditional))
            flags |= VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT;

        return flags;
    }
}

Buffer::Builder::Builder(const BufferDescription& description)
{
    SetUsage(description.Usage);
    m_CreateInfo.Description = description;
}

Buffer Buffer::Builder::Build()
{
    Buffer buffer = Buffer::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([buffer](){ Buffer::Destroy(buffer); });

    return buffer;
}

Buffer Buffer::Builder::BuildManualLifetime()
{
    return Buffer::Create(m_CreateInfo);
}

Buffer::Builder& Buffer::Builder::SetUsage(BufferUsage usage)
{
    m_CreateInfo.UsageFlags |= vulkanBufferUsageFromUsage(usage);
    m_CreateInfo.Description.Usage |=  usage;

    if (enumHasAny(usage, BufferUsage::Upload))
        m_CreateInfo.MemoryUsage = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    if (enumHasAny(usage, BufferUsage::UploadRandomAccess | BufferUsage::Readback))
        m_CreateInfo.MemoryUsage = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    
    return *this;
}

Buffer::Builder& Buffer::Builder::SetSizeBytes(u64 sizeBytes)
{
    m_CreateInfo.Description.SizeBytes = sizeBytes;

    return *this;
}

Buffer Buffer::Create(const Builder::CreateInfo& createInfo)
{
    Buffer buffer = {};

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.usage = createInfo.UsageFlags;
    bufferCreateInfo.size = createInfo.Description.SizeBytes;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = createInfo.MemoryUsage;

    VulkanCheck(vmaCreateBuffer(Driver::Allocator(), &bufferCreateInfo, &allocationCreateInfo,
        &buffer.m_Buffer, &buffer.m_Allocation, nullptr),
        "Failed to create a buffer");

    buffer.m_Description = createInfo.Description;

    return buffer;
}

void Buffer::Destroy(const Buffer& buffer)
{
    vmaDestroyBuffer(Driver::Allocator(), buffer.m_Buffer, buffer.m_Allocation);
}

void Buffer::SetData(const void* data, u64 dataSizeBytes)
{
    void* mappedData = nullptr;
    vmaMapMemory(Driver::Allocator(), m_Allocation, &mappedData);
    std::memcpy(mappedData, data, dataSizeBytes);
    vmaUnmapMemory(Driver::Allocator(), m_Allocation);
}

void Buffer::SetData(const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    void* mappedData = nullptr;
    vmaMapMemory(Driver::Allocator(), m_Allocation, &mappedData);
    mappedData = (void*)((u8*)mappedData + offsetBytes);
    std::memcpy(mappedData, data, dataSizeBytes);
    vmaUnmapMemory(Driver::Allocator(), m_Allocation);
}

void Buffer::SetData(void* mapped, const void* data, u64 dataSizeBytes, u64 offsetBytes)
{
    mapped = (void*)((u8*)mapped + offsetBytes);
    std::memcpy(mapped, data, dataSizeBytes);
}

void* Buffer::Map() const
{
    void* mappedData;
    vmaMapMemory(Driver::Allocator(), m_Allocation, &mappedData);
    return mappedData;
}

void Buffer::Unmap() const
{
    vmaUnmapMemory(Driver::Allocator(), m_Allocation);
}

BufferSubresource Buffer::CreateSubresource(u64 sizeBytes, u64 offset) const
{
    ASSERT(offset + sizeBytes < m_Description.SizeBytes, "Invalid subresource range")
    return {
        .Buffer = this,
        .SizeBytes = sizeBytes,
        .Offset = offset};
}
