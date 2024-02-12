#pragma once

#include <optional>
#include <unordered_map>

#include "VulkanCommon.h"
#include "types.h"

#include <vector>
#include <Vulkan/vulkan_core.h>

struct ImageBindingInfo;
struct BufferSubresource;
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
            std::vector<VkDescriptorBindingFlags> BindingFlags;
            VkDescriptorSetLayoutCreateFlags Flags;
        };
    public:
        DescriptorSetLayout Build();
        DescriptorSetLayout BuildManualLifetime();
        Builder& SetBindings(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
        Builder& SetBindingFlags(const std::vector<VkDescriptorBindingFlags>& flags);
        Builder& SetFlags(VkDescriptorSetLayoutCreateFlags flags);
    private:
        void PreBuild();
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
    using BufferBindingInfo = BufferSubresource;
    using TextureBindingInfo = ImageBindingInfo;
    class Builder
    {
        friend class DescriptorSet;
        friend class DescriptorLayoutCache;
        FRIEND_INTERNAL
        struct VariableBindingInfo
        {
            u32 Slot;
            u32 Count;
        };
        struct CreateInfo
        {
            struct BoundResource
            {
                std::optional<VkDescriptorBufferInfo> Buffer;
                std::optional<VkDescriptorImageInfo> Texture;
                u32 Slot;
                VkDescriptorType Type;
            };

            VkDescriptorPoolCreateFlags PoolFlags{0};
            std::vector<BoundResource> BoundResources;
            DescriptorAllocator* Allocator;
            const DescriptorSetLayout* Layout;
            VkDescriptorSetVariableDescriptorCountAllocateInfo VariableDescriptorCounts;
        };
    public:
        DescriptorSet Build();
        Builder& SetAllocator(DescriptorAllocator* allocator);
        Builder& SetLayout(const DescriptorSetLayout* layout);
        Builder& SetPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder& AddBufferBinding(u32 slot, const BufferBindingInfo& bindingInfo, VkDescriptorType descriptor);
        Builder& AddTextureBinding(u32 slot, const TextureBindingInfo& texture, VkDescriptorType descriptor);
        Builder& AddVariableBinding(const VariableBindingInfo& variableBindingInfo);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
        std::vector<u32> m_VariableBindingSlots;
        std::vector<u32> m_VariableBindingCounts;
    };
public:
    static DescriptorSet Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorSet& descriptorSet);

    void Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint);
    void Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint,
        const std::vector<u32>& dynamicOffsets);

    void SetTexture(u32 slot, const Texture& texture, VkDescriptorType descriptor, u32 arrayIndex);
    
    const DescriptorSetLayout* GetLayout() const { return m_Layout; }
    bool IsValid() const { return m_DescriptorSet != VK_NULL_HANDLE; }
private:
    VkDescriptorSet m_DescriptorSet{VK_NULL_HANDLE};
    DescriptorAllocator* m_Allocator{nullptr};
    VkDescriptorPool m_Pool{VK_NULL_HANDLE};
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
    struct PoolInfo
    {
        VkDescriptorPool Pool;
        VkDescriptorPoolCreateFlags Flags;
        u32 AllocationCount{0};
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

    void Allocate(DescriptorSet& set, VkDescriptorPoolCreateFlags poolFlags,
        const VkDescriptorSetVariableDescriptorCountAllocateInfo& variableDescriptorCounts);
    void Deallocate(const DescriptorSet& set);

    void ResetPools();
private:
    u32 GrabPool(VkDescriptorPoolCreateFlags poolFlags);
    PoolInfo CreatePool(VkDescriptorPoolCreateFlags poolFlags);
    
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
    std::vector<PoolInfo> m_FreePools;
    std::vector<PoolInfo> m_UsedPools;
};

// todo: make me static
class DescriptorLayoutCache
{
    friend class ShaderPipelineTemplate;
    struct CacheKey
    {
        std::vector<VkDescriptorSetLayoutBinding> Bindings;
        std::vector<VkDescriptorBindingFlags> BindingFlags;
        VkDescriptorSetLayoutCreateFlags Flags;
        bool operator==(const CacheKey& other) const;
    };
private:
    DescriptorSetLayout* CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings,
        const std::vector<VkDescriptorBindingFlags>& bindingFlags, VkDescriptorSetLayoutCreateFlags layoutFlags);
    void SortBindings(CacheKey& cacheKey);
private:
    struct DescriptorSetLayoutKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };
    
    std::unordered_map<CacheKey, DescriptorSetLayout, DescriptorSetLayoutKeyHash> m_LayoutCache;
};