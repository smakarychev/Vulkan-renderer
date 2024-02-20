#pragma once

#include <optional>
#include <unordered_map>

#include "RenderingCommon.h"
#include "types.h"

#include <vector>

#include "Buffer.h"
#include "DescriptorSetTraits.h"
#include "Image.h"

class Sampler;
class PipelineLayout;
class Image;
class CommandBuffer;
class Pipeline;
class Buffer;
class DescriptorSetLayout;
class DescriptorPool;
class Device;

struct DescriptorSetBinding
{
    u32 Binding;
    DescriptorType Type;
    u32 Count;
    ShaderStage Shaders;
    bool HasImmutableSampler{false};
};

class DescriptorSetLayout
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class DescriptorSetLayout;
        friend class DescriptorLayoutCache;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<DescriptorSetBinding> Bindings;
            std::vector<DescriptorFlags> BindingFlags;
            DescriptorSetFlags Flags;
        };
    public:
        DescriptorSetLayout Build();
        Builder& SetBindings(const std::vector<DescriptorSetBinding>& bindings);
        Builder& SetBindingFlags(const std::vector<DescriptorFlags>& flags);
        Builder& SetFlags(DescriptorSetFlags flags);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorSetLayout Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorSetLayout& layout);
private:
    ResourceHandle<DescriptorSetLayout> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandle<DescriptorSetLayout> m_ResourceHandle;
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
                std::optional<BufferBindingInfo> Buffer;
                std::optional<TextureBindingInfo> Texture;
                u32 Slot;
                DescriptorType Type;
            };

            DescriptorPoolFlags PoolFlags{0};
            std::vector<BoundResource> BoundResources;
            u32 BoundBufferCount{0};
            u32 BoundTextureCount{0};
            DescriptorAllocator* Allocator;
            DescriptorSetLayout Layout;
            std::vector<u32> VariableBindingSlots;
            std::vector<u32> VariableBindingCounts;
        };
    public:
        DescriptorSet Build();
        DescriptorSet Build(DeletionQueue& deletionQueue);
        DescriptorSet BuildManualLifetime();
        
        Builder& SetAllocator(DescriptorAllocator* allocator);
        Builder& SetLayout(DescriptorSetLayout layout);
        Builder& SetPoolFlags(DescriptorPoolFlags flags);
        Builder& AddBufferBinding(u32 slot, const BufferBindingInfo& bindingInfo, DescriptorType descriptor);
        Builder& AddTextureBinding(u32 slot, const TextureBindingInfo& texture, DescriptorType descriptor);
        Builder& AddVariableBinding(const VariableBindingInfo& variableBindingInfo);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorSet Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorSet& descriptorSet);

    void BindGraphics(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex);
    void BindGraphics(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex,
        const std::vector<u32>& dynamicOffsets);
    void BindCompute(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex);
    void BindCompute(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex,
        const std::vector<u32>& dynamicOffsets);

    void SetTexture(u32 slot, const Texture& texture, DescriptorType descriptor, u32 arrayIndex);
    
    const DescriptorSetLayout& GetLayout() const { return m_Layout; }
private:
    ResourceHandle<DescriptorSet> Handle() const { return m_ResourceHandle; }
private:
    DescriptorAllocator* m_Allocator{nullptr};
    DescriptorSetLayout m_Layout;
    ResourceHandle<DescriptorSet> m_ResourceHandle;
};

class DescriptorAllocator
{
    friend class DescriptorSet;
    FRIEND_INTERNAL
    struct PoolSize
    {
        DescriptorType DescriptorType;
        f32 SetSizeMultiplier;
    };
    using PoolSizes = std::vector<PoolSize>;
public:
    class Builder
    {
        friend class DescriptorAllocator;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            u32 MaxSets;
        };
    public:
        DescriptorAllocator Build();
        DescriptorAllocator Build(DeletionQueue& deletionQueue);
        DescriptorAllocator BuildManualLifetime();
        Builder& SetMaxSetsPerPool(u32 maxSets);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorAllocator Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorAllocator& allocator);

    void Allocate(DescriptorSet& set, DescriptorPoolFlags poolFlags, const std::vector<u32>& variableBindingCounts);
    void Deallocate(const DescriptorSet& set);

    void ResetPools();
private:
    ResourceHandle<DescriptorAllocator> Handle() const { return m_ResourceHandle; }
private:
    PoolSizes m_PoolSizes = {
        { DescriptorType::Sampler, 0.5f },
        { DescriptorType::ImageSampler, 4.f },
        { DescriptorType::Image, 4.f },
        { DescriptorType::ImageStorage, 1.f },
        { DescriptorType::TexelUniform, 1.f },
        { DescriptorType::TexelStorage, 1.f },
        { DescriptorType::UniformBuffer, 2.f },
        { DescriptorType::StorageBuffer, 2.f },
        { DescriptorType::UniformBufferDynamic, 1.f },
        { DescriptorType::StorageBufferDynamic, 1.f },
        { DescriptorType::Input, 0.5f }
    };

    u32 m_MaxSetsPerPool{};
    ResourceHandle<DescriptorAllocator> m_ResourceHandle;
};

class DescriptorLayoutCache
{
    friend class ShaderPipelineTemplate;
    struct CacheKey
    {
        DescriptorSetLayout::Builder::CreateInfo CreateInfo;
        bool operator==(const CacheKey& other) const;
    };
public:
    static DescriptorSetLayout CreateDescriptorSetLayout(const DescriptorSetLayout::Builder::CreateInfo& createInfo);
private:
    static void SortBindings(CacheKey& cacheKey);
private:
    struct DescriptorSetLayoutKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };
    
    static std::unordered_map<CacheKey, DescriptorSetLayout, DescriptorSetLayoutKeyHash> s_LayoutCache;
};