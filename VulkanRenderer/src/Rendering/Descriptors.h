#pragma once

#include <optional>
#include <unordered_map>

#include "RenderingCommon.h"
#include "types.h"

#include <vector>

#include "Buffer.h"
#include "DescriptorsTraits.h"
#include "Image/Image.h"
#include "ShaderAsset.h"

namespace assetLib
{
    struct ShaderInfo;
}

class DescriptorArenaAllocators;
class DescriptorArenaAllocator;
class ResourceUploader;
class Sampler;
class PipelineLayout;
class Image;
class CommandBuffer;
class Pipeline;
class Buffer;
class DescriptorsLayout;
class DescriptorPool;
class Device;

struct DescriptorBinding
{
    using Flags = assetLib::ShaderInfo::DescriptorSet::DescriptorFlags;
    u32 Binding;
    DescriptorType Type;
    u32 Count;
    ShaderStage Shaders;
    Flags DescriptorFlags{Flags::None};
};

class DescriptorsLayout
{
    FRIEND_INTERNAL
public:
    class Builder
    {
        friend class DescriptorsLayout;
        friend class DescriptorLayoutCache;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            std::vector<DescriptorBinding> Bindings;
            std::vector<DescriptorFlags> BindingFlags;
            DescriptorLayoutFlags Flags;
        };
    public:
        DescriptorsLayout Build();
        Builder& SetBindings(const std::vector<DescriptorBinding>& bindings);
        Builder& SetBindingFlags(const std::vector<DescriptorFlags>& flags);
        Builder& SetFlags(DescriptorLayoutFlags flags);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorsLayout Create(const Builder::CreateInfo& createInfo);
    static void Destroy(const DescriptorsLayout& layout);
private:
    ResourceHandleType<DescriptorsLayout> Handle() const { return m_ResourceHandle; }
private:
    ResourceHandleType<DescriptorsLayout> m_ResourceHandle{};
};

class DescriptorSet
{
    using Texture = Image;
    FRIEND_INTERNAL
    friend class DescriptorAllocator;
public:
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
            template <typename Binding>
            struct BoundResource
            {
                Binding BindingInfo;
                u32 Slot;
                DescriptorType Type;
            };
            using BoundBuffer = BoundResource<BufferBindingInfo>;
            using BoundTexture = BoundResource<TextureBindingInfo>;

            DescriptorPoolFlags PoolFlags{0};
            std::vector<BoundBuffer> Buffers;
            std::vector<BoundTexture> Textures;
            DescriptorAllocator* Allocator;
            DescriptorsLayout Layout;
            std::vector<u32> VariableBindingSlots;
            std::vector<u32> VariableBindingCounts;
        };
    public:
        DescriptorSet Build();
        
        Builder& SetAllocator(DescriptorAllocator* allocator);
        Builder& SetLayout(DescriptorsLayout layout);
        Builder& SetPoolFlags(DescriptorPoolFlags flags);
        Builder& AddBufferBinding(u32 slot, const BufferBindingInfo& bindingInfo, DescriptorType descriptor);
        Builder& AddTextureBinding(u32 slot, const TextureBindingInfo& texture, DescriptorType descriptor);
        Builder& AddVariableBinding(const VariableBindingInfo& variableBindingInfo);
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorSet Create(const Builder::CreateInfo& createInfo);

    void BindGraphics(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex);
    void BindGraphics(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex,
        const std::vector<u32>& dynamicOffsets);
    void BindCompute(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex);
    void BindCompute(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex,
        const std::vector<u32>& dynamicOffsets);

    void SetTexture(u32 slot, const Texture& texture, DescriptorType descriptor, u32 arrayIndex);
    
    const DescriptorsLayout& GetLayout() const { return m_Layout; }
private:
    ResourceHandleType<DescriptorSet> Handle() const { return m_ResourceHandle; }
private:
    DescriptorAllocator* m_Allocator{nullptr};
    DescriptorsLayout m_Layout;
    ResourceHandleType<DescriptorSet> m_ResourceHandle{};
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
    void Deallocate(ResourceHandleType<DescriptorSet> set);

    void ResetPools();
private:
    ResourceHandleType<DescriptorAllocator> Handle() const { return m_ResourceHandle; }
private:
    PoolSizes m_PoolSizes = {
        { DescriptorType::Sampler, 0.5f },
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
    ResourceHandleType<DescriptorAllocator> m_ResourceHandle{};
};

class Descriptors
{
    FRIEND_INTERNAL
public:
    struct BindingInfo
    {
        u32 Slot;
        DescriptorType Type;
    };
public:
    using BufferBindingInfo = BufferSubresource;
    using TextureBindingInfo = ImageBindingInfo;

    void UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const;
    void UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture, u32 index) const;

    /* 'Global' versions of UpdateBinding updates bindings in ALL descriptor buffers */
    
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const;
    void UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture, u32 index) const;
    
    void BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, u32 firstSet) const;
    void BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
        PipelineLayout pipelineLayout, u32 firstSet) const;
    void BindGraphicsImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 firstSet) const;
    void BindComputeImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 firstSet) const;
private:
    std::vector<u64> m_Offsets;
    u64 m_SizeBytes{0};
    const DescriptorArenaAllocator* m_Allocator;
};

enum class DescriptorAllocatorKind
{
    Resources = 0, Samplers,
    MaxVal
};

enum class DescriptorAllocatorResidence
{
    CPU, GPU
};

struct DescriptorAllocatorAllocationBindings
{
    std::vector<DescriptorBinding> Bindings;
    /* used to specify the count of bindless descriptors,
     * for each set only one descriptor can be bindless, and it is always the last one
     */
    u32 BindlessCount{0};
};

// todo: name is temp, `DescriptorAllocator` is currently an existing entity
class DescriptorArenaAllocator
{
    FRIEND_INTERNAL
    friend class DescriptorArenaAllocators;
public:
    class Builder
    {
        friend class DescriptorArenaAllocator;
        FRIEND_INTERNAL
        struct CreateInfo
        {
            DescriptorAllocatorKind Kind{DescriptorAllocatorKind::Resources};
            DescriptorAllocatorResidence Residence{DescriptorAllocatorResidence::CPU};
            std::vector<DescriptorType> UsedTypes;
            u32 DescriptorCount;
        };
    public:
        DescriptorArenaAllocator Build();
        DescriptorArenaAllocator Build(DeletionQueue& deletionQueue);
        Builder& Kind(DescriptorAllocatorKind kind);
        Builder& Residence(DescriptorAllocatorResidence residence);
        Builder& Count(u32 count);
        Builder& ForTypes(const std::vector<DescriptorType>& types);
    private:
        void PreBuild();
    private:
        CreateInfo m_CreateInfo;
    };
public:
    static DescriptorArenaAllocator Create(const Builder::CreateInfo& createInfo);

    Descriptors Allocate(DescriptorsLayout layout, const DescriptorAllocatorAllocationBindings& bindings);
    void Reset();

    /* `bufferIndex` is usually a frame number from frame context (between 0 and BUFFERED_FRAMES)
     * NOTE: usually you want to call `Bind` on `DescriptorArenaAllocators`
     */
    void Bind(const CommandBuffer& cmd, u32 bufferIndex);
private:
    void ValidateBindings(const DescriptorAllocatorAllocationBindings& bindings) const;

    const Buffer& GetCurrentBuffer() const { return m_Buffers[m_CurrentBuffer]; }
private:
    std::array<Buffer, BUFFERED_FRAMES> m_Buffers;
    u32 m_CurrentBuffer{0};
    u64 m_CurrentOffset{0};
    DescriptorAllocatorKind m_Kind{DescriptorAllocatorKind::Resources};
    DescriptorAllocatorResidence m_Residence{DescriptorAllocatorResidence::CPU};
    std::vector<DescriptorType> m_UsedTypes;
};

class DescriptorArenaAllocators
{
    FRIEND_INTERNAL
public:
    DescriptorArenaAllocators(const DescriptorArenaAllocator& resourceAllocator,
        const DescriptorArenaAllocator& samplerAllocator);
    
    const DescriptorArenaAllocator& Get(DescriptorAllocatorKind kind) const;
    DescriptorArenaAllocator& Get(DescriptorAllocatorKind kind);

    /* `bufferIndex` is usually a frame number from frame context (between 0 and BUFFERED_FRAMES) */
    void Bind(const CommandBuffer& cmd, u32 bufferIndex);
private:
    std::array<DescriptorArenaAllocator, (u32)DescriptorAllocatorKind::MaxVal> m_Allocators;
};

class DescriptorLayoutCache
{
    friend class ShaderPipelineTemplate;
    struct CacheKey
    {
        DescriptorsLayout::Builder::CreateInfo CreateInfo;
        bool operator==(const CacheKey& other) const;
    };
public:
    static DescriptorsLayout CreateDescriptorSetLayout(const DescriptorsLayout::Builder::CreateInfo& createInfo);
private:
    static void SortBindings(CacheKey& cacheKey);
private:
    struct DescriptorSetLayoutKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };
    
    static std::unordered_map<CacheKey, DescriptorsLayout, DescriptorSetLayoutKeyHash> s_LayoutCache;
};