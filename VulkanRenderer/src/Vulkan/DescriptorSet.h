#pragma once

#include <unordered_map>

#include "VulkanCommon.h"
#include "types.h"

#include <vector>
#include <vulkan/vulkan_core.h>

class PipelineLayout;
class Image;
class CommandBuffer;
class Pipeline;
class Buffer;
class DescriptorSetLayout;
class DescriptorPool;
class Device;

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
        Builder& SetBindings(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
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

class DescriptorSet
{
    using Texture = Image;
    FRIEND_INTERNAL
    friend class DescriptorAllocator;
public:
    struct BufferBindingInfo
    {
        const Buffer* Buffer{nullptr};
        u64 SizeBytes{0};
        u64 OffsetBytes{0};
    };
    class Builder
    {
        friend class DescriptorSet;
        friend class DescriptorLayoutCache;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            template <typename T>
            struct BoundResource
            {
                T ResourceInfo;
                u32 Slot;
            };
            using BoundBuffer = BoundResource<VkDescriptorBufferInfo>;
            using BoundTexture = BoundResource<VkDescriptorImageInfo>;

            std::vector<VkDescriptorSetLayoutBinding> Bindings;
            std::vector<BoundBuffer> BoundBuffers;
            std::vector<BoundTexture> BoundTextures;
            DescriptorAllocator* Allocator;
            DescriptorLayoutCache* Cache;
        };
    public:
        DescriptorSet Build();
        Builder& SetAllocator(DescriptorAllocator* allocator);
        Builder& SetLayoutCache(DescriptorLayoutCache* cache);
        Builder& AddBufferBinding(u32 slot, const BufferBindingInfo& bindingInfo, VkDescriptorType descriptor, VkShaderStageFlags stages);
        Builder& AddTextureBinding(u32 slot, const Texture& texture, VkDescriptorType descriptor, VkShaderStageFlags stages);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorSet Create(const Builder::CreateInfo& createInfo);
    
    void Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint);
    void Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint,
        const std::vector<u32>& dynamicOffsets);
    const DescriptorSetLayout* GetLayout() const { return m_Layout; }
    bool IsValid() const { return m_DescriptorSet != VK_NULL_HANDLE; }
private:
    VkDescriptorSet m_DescriptorSet{VK_NULL_HANDLE};
    const DescriptorSetLayout* m_Layout{nullptr};
};

class DescriptorAllocator
{
    FRIEND_INTERNAL
    struct PoolSize
    {
        VkDescriptorType DescriptorType;
        f32 SetSizeMultiplier;
    };
    using PoolSizes = std::vector<PoolSize>;
    
    struct SetAllocateInfo
    {
        VkDescriptorSetAllocateInfo Info;
    };
public:
    class Builder
    {
        friend class DescriptorAllocator;
        struct CreateInfo
        {
            u32 MaxSets;
        };
    public:
        DescriptorAllocator Build();
        DescriptorAllocator BuildManualLifetime();
        Builder& SetMaxSetsPerPool(u32 maxSets);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorAllocator Create(const Builder::CreateInfo& createInfo);

    void Allocate(DescriptorSet& set);

    void ResetPools();
private:
    VkDescriptorPool GrabPool();
    VkDescriptorPool CreatePool();
    
private:
    PoolSizes m_PoolSizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 0.5f },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0.5f }
    };

    u32 m_MaxSetsPerPool{};
    std::vector<VkDescriptorPool> m_FreePools;
    std::vector<VkDescriptorPool> m_UsedPools;
};

class DescriptorLayoutCache
{
    struct CacheKey
    {
        std::vector<VkDescriptorSetLayoutBinding> Bindings;
        bool operator==(const CacheKey& other) const;
    };
public:
    DescriptorSetLayout* CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
private:
    void SortBindings(CacheKey& cacheKey);
private:
    struct DescriptorSetLayoutCreateInfoHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };
    
    std::unordered_map<CacheKey, DescriptorSetLayout, DescriptorSetLayoutCreateInfoHash> m_LayoutCache;
};