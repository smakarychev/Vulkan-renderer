﻿#include "Buffer.h"

#include "Driver.h"
#include "RenderCommand.h"
#include "VulkanUtils.h"

namespace
{
    
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

Buffer::Builder& Buffer::Builder::SetKind(BufferKind kind)
{
    m_CreateInfo.UsageFlags |= vkUtils::vkBufferUsageByKind(kind);
    m_CreateInfo.Kind = kind;
    
    return *this;
}

Buffer::Builder& Buffer::Builder::SetKinds(const std::vector<BufferKind>& kinds)
{
    for (auto& kind : kinds)
        SetKind(kind);

    return *this;
}

Buffer::Builder& Buffer::Builder::SetMemoryUsage(VmaMemoryUsage usage)
{
    m_CreateInfo.MemoryUsage = usage;

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
    allocationCreateInfo.usage = createInfo.MemoryUsage;

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
    RenderCommand::BindBuffer(commandBuffer, *this, offset);
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

PushConstantDescription PushConstantDescription::Builder::Build()
{
    return PushConstantDescription::Create(m_CreateInfo);
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetSizeBytes(u32 sizeBytes)
{
    m_CreateInfo.SizeBytes = sizeBytes;

    return *this;
}

PushConstantDescription::Builder& PushConstantDescription::Builder::SetStages(VkShaderStageFlagBits stages)
{
    m_CreateInfo.Stages = stages;

    return *this;
}

PushConstantDescription PushConstantDescription::Create(const Builder::CreateInfo& createInfo)
{
    PushConstantDescription pushConstantDescription = {};

    pushConstantDescription.m_SizeBytes = createInfo.SizeBytes;
    pushConstantDescription.m_StageFlags = createInfo.Stages;

    return  pushConstantDescription;
}