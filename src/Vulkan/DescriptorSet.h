#pragma once

#include "VulkanCommon.h"
#include "types.h"

#include <vector>
#include <vulkan/vulkan_core.h>

class CommandBuffer;
class Pipeline;
class Buffer;
class DescriptorSetLayout;
class DescriptorPool;
class Device;

class DescriptorSet
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class DescriptorSet;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            VkDescriptorPool Pool;
            VkDescriptorSetLayout LayoutHandle;
            const DescriptorSetLayout* Layout;
        };
    public:
        DescriptorSet Build();
        Builder& SetPool(const DescriptorPool& pool);
        Builder& SetLayout(const DescriptorSetLayout& layout);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorSet Create(const Builder::CreateInfo& createInfo);
    void BindBuffer(u32 slot, const Buffer& buffer, u64 sizeBytes);
    void BindBuffer(u32 slot, const Buffer& buffer, u64 sizeBytes, u64 offsetBytes);
    void Bind(const CommandBuffer& commandBuffer, const Pipeline& pipeline, VkPipelineBindPoint bindPoint);
    void Bind(const CommandBuffer& commandBuffer, const Pipeline& pipeline, VkPipelineBindPoint bindPoint,
        const std::vector<u32>& dynamicOffsets);
    const DescriptorSetLayout* GetLayout() const { return m_Layout; }
private:
    VkDescriptorSet m_DescriptorSet{VK_NULL_HANDLE};
    const DescriptorSetLayout* m_Layout{nullptr};
};


class DescriptorPool
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class DescriptorPool;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<VkDescriptorPoolSize> Sizes;
            u32 MaxSets;
        };
    public:
        DescriptorPool Build();
        DescriptorPool BuildManualLifetime();
        Builder& Defaults();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorPool Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorPool& pool);

    DescriptorSet Allocate(const DescriptorSetLayout& layout);
private:
    VkDescriptorPool m_Pool{VK_NULL_HANDLE};
};

class DescriptorSetLayout
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class DescriptorSetLayout;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<VkDescriptorSetLayoutBinding> Bindings;
        };
    public:
        DescriptorSetLayout Build();
        DescriptorSetLayout BuildManualLifetime();
        Builder& AddBinding(VkDescriptorType type, VkShaderStageFlags stage);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorSetLayout Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorSetLayout& layout);
private:
    std::vector<VkDescriptorType> m_Descriptors;
    VkDescriptorSetLayout m_Layout{VK_NULL_HANDLE};
};

