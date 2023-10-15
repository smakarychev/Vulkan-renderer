#include "Buffer.h"

#include "Driver.h"
#include "RenderCommand.h"
#include "VulkanCore.h"
#include "VulkanUtils.h"

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

Buffer::Builder& Buffer::Builder::SetKind(BufferKind kind)
{
    m_CreateInfo.UsageFlags |= vkUtils::vkBufferUsageByKind(kind);
    m_CreateInfo.Kind.Kind |=  kind.Kind;
    
    return *this;
}

Buffer::Builder& Buffer::Builder::SetKinds(const std::vector<BufferKind>& kinds)
{
    for (auto& kind : kinds)
        SetKind(kind);

    return *this;
}

Buffer::Builder& Buffer::Builder::SetMemoryFlags(VmaAllocationCreateFlags flags)
{
    m_CreateInfo.MemoryUsage |= flags;

    return *this;
}

Buffer::Builder& Buffer::Builder::SetSizeBytes(u64 sizeBytes)
{
    m_CreateInfo.SizeBytes = sizeBytes;

    return *this;
}

Buffer Buffer::Create(const Builder::CreateInfo& createInfo)
{
    Buffer buffer = {};

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.usage = createInfo.UsageFlags;
    bufferCreateInfo.size = createInfo.SizeBytes;

    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationCreateInfo.flags = createInfo.MemoryUsage;

    VulkanCheck(vmaCreateBuffer(Driver::Allocator(), &bufferCreateInfo, &allocationCreateInfo, &buffer.m_Buffer, &buffer.m_Allocation, nullptr),
        "Failed to create a buffer");

    buffer.m_Kind = createInfo.Kind;
    buffer.m_SizeBytes = createInfo.SizeBytes;

    return buffer;
}

void Buffer::Destroy(const Buffer& buffer)
{
    vmaDestroyBuffer(Driver::Allocator(), buffer.m_Buffer, buffer.m_Allocation);
}

void Buffer::Bind(const CommandBuffer& commandBuffer, u64 offset) const
{
    if (m_Kind.Kind & BufferKind::Vertex)
        RenderCommand::BindVertexBuffer(commandBuffer, *this, offset);
    else if (m_Kind.Kind & BufferKind::Index)
        RenderCommand::BindIndexBuffer(commandBuffer, *this, offset);
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

void* Buffer::Map()
{
    void* mappedData;
    vmaMapMemory(Driver::Allocator(), m_Allocation, &mappedData);
    return mappedData;
}

void Buffer::Unmap()
{
    vmaUnmapMemory(Driver::Allocator(), m_Allocation);
}

PushConstantDescription PushConstantDescription::Builder::Build()
{
    return PushConstantDescription::Create(m_CreateInfo);
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetSizeBytes(u32 sizeBytes)
{
    m_CreateInfo.SizeBytes = sizeBytes;

    return *this;
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetOffset(u32 offset)
{
    m_CreateInfo.Offset = offset;

    return *this;
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetStages(VkShaderStageFlags stages)
{
    m_CreateInfo.Stages = stages;

    return *this;
}

PushConstantDescription PushConstantDescription::Create(const Builder::CreateInfo& createInfo)
{
    PushConstantDescription pushConstantDescription = {};

    pushConstantDescription.m_SizeBytes = createInfo.SizeBytes;
    pushConstantDescription.m_Offset = createInfo.Offset;
    pushConstantDescription.m_StageFlags = createInfo.Stages;

    return  pushConstantDescription;
}
