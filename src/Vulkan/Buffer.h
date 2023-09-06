#pragma once

#include "types.h"
#include "VulkanCommon.h"

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

enum class BufferKind{Vertex, Index};

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
            VmaMemoryUsage MemoryUsage;
            u64 SizeBytes;
            BufferKind Kind;
        };
    public:
        Buffer Build();
        Builder& SetKind(BufferKind kind); // all my buffers are kind
        Builder& SetMemoryUsage(VmaMemoryUsage usage);
        Builder& SetSizeBytes(u64 sizeBytes);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static Buffer Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const Buffer& buffer);

    void SetData(const void* data, u64 dataSizeBytes);
private:
    VkBuffer m_Buffer{VK_NULL_HANDLE};
    VmaAllocation m_Allocation{VK_NULL_HANDLE};
    BufferKind m_Kind{};
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
            VkShaderStageFlagBits Stages;
            u32 SizeBytes;
        };
    public:
        PushConstantDescription Build();
        Builder& SetSizeBytes(u32 sizeBytes);
        Builder& SetStages(VkShaderStageFlagBits stages);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static PushConstantDescription Create(const Builder::CreateInfo& createInfo);
private:
    VkShaderStageFlags m_StageFlags{};
    u32 m_SizeBytes{};
};