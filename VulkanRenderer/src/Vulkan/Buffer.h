#pragma once

#include "types.h"
#include "VulkanCommon.h"

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

class CommandBuffer;

class Buffer
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class Buffer;
        struct CreateInfo
        {
            VkBufferUsageFlags UsageFlags;
            VmaAllocationCreateFlags MemoryUsage;
            u64 SizeBytes;
            BufferKind Kind;
        };
    public:
        Buffer Build();
        Buffer BuildManualLifetime();
        Builder& SetKind(BufferKind kind); // all my buffers are kind
        Builder& SetKinds(const std::vector<BufferKind>& kinds);
        Builder& SetMemoryFlags(VmaAllocationCreateFlags flags);
        Builder& SetSizeBytes(u64 sizeBytes);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Buffer Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Buffer& buffer);

    void Bind(const CommandBuffer& commandBuffer, u64 offset = 0) const;
    void SetData(const void* data, u64 dataSizeBytes);
    void SetData(const void* data, u64 dataSizeBytes, u64 offsetBytes);
    void SetData(void* mapped, const void* data, u64 dataSizeBytes, u64 offsetBytes);
    void* Map();
    void Unmap();
    u64 GetSizeBytes() const { return m_SizeBytes; }
    BufferKind GetKind() const { return m_Kind; }
private:
    VkBuffer m_Buffer{VK_NULL_HANDLE};
    VmaAllocation m_Allocation{VK_NULL_HANDLE};
    BufferKind m_Kind{};
    u64 m_SizeBytes{};
};

class PushConstantDescription
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class PushConstantDescription;
        struct CreateInfo
        {
            VkShaderStageFlags Stages;
            u32 SizeBytes;
            u32 Offset{0};
        };
    public:
        PushConstantDescription Build();
        Builder& SetSizeBytes(u32 sizeBytes);
        Builder& SetOffset(u32 offset);
        Builder& SetStages(VkShaderStageFlags stages);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static PushConstantDescription Create(const Builder::CreateInfo& createInfo);
private:
    u32 m_SizeBytes{};
    u32 m_Offset{};
    VkShaderStageFlags m_StageFlags{};
};