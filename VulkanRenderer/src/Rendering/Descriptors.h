#pragma once

#include "RenderingCommon.h"

#include "Buffer.h"
#include "DescriptorsTraits.h"
#include "Image/Image.h"
#include "ShaderAsset.h"

#include <vector>
#include <unordered_map>

namespace assetLib
{
    struct ShaderStageInfo;
}

class DescriptorArenaAllocators;
class DescriptorArenaAllocator;
class ResourceUploader;
class Sampler;
class Image;
class CommandBuffer;
class Buffer;
class DescriptorPool;

struct DescriptorBinding
{
    using Flags = assetLib::ShaderStageInfo::DescriptorSet::DescriptorFlags;
    u32 Binding;
    DescriptorType Type;
    u32 Count;
    ShaderStage Shaders;
    Flags DescriptorFlags{Flags::None};

    auto operator<=>(const DescriptorBinding&) const = default;
};

struct DescriptorsLayoutCreateInfo
{
    Span<const DescriptorBinding> Bindings{};
    Span<const DescriptorFlags> BindingFlags{};
    DescriptorLayoutFlags Flags{DescriptorLayoutFlags::None};
};

struct DescriptorAllocatorCreateInfo
{
    u32 MaxSets{0};
};

struct DescriptorAllocatorTag{};
using DescriptorAllocator = ResourceHandleType<DescriptorAllocatorTag>;

struct DescriptorsLayoutTag {};
using DescriptorsLayout = ResourceHandleType<DescriptorsLayoutTag>;

struct DescriptorSetCreateInfo
{
    struct VariableBindingInfo
    {
        u32 Slot;
        u32 Count;
    };
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
    std::span<const BoundBuffer> Buffers;
    std::span<const BoundTexture> Textures;
    DescriptorAllocator Allocator{};
    DescriptorsLayout Layout{};
    std::span<const VariableBindingInfo> VariableBindings;
};

struct DescriptorSetTag {};
using DescriptorSet = ResourceHandleType<DescriptorSetTag>;

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

struct DescriptorArenaAllocatorCreateInfo
{
    DescriptorAllocatorKind Kind{DescriptorAllocatorKind::Resources};
    DescriptorAllocatorResidence Residence{DescriptorAllocatorResidence::CPU};
    Span<const DescriptorType> UsedTypes;
    u32 DescriptorCount{0};
};

class DescriptorArenaAllocator
{
    FRIEND_INTERNAL
    friend class DescriptorArenaAllocators;
public:
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
public:
    class CacheKey
    {
        friend class DescriptorLayoutCache;
    public:
        auto operator<=>(const CacheKey&) const = default;
    private:
        std::vector<DescriptorBinding> m_Bindings{};
        std::vector<DescriptorFlags> m_BindingFlags{};
        DescriptorLayoutFlags m_Flags{DescriptorLayoutFlags::None};
    };
public:
    static CacheKey CreateCacheKey(const DescriptorsLayoutCreateInfo& createInfo);
    static DescriptorsLayout Find(const CacheKey& key);
    static void Emplace(const CacheKey& key, DescriptorsLayout layout);
private:
    static void SortBindings(CacheKey& cacheKey);
private:
    struct DescriptorSetLayoutKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };
    
    static std::unordered_map<CacheKey, DescriptorsLayout, DescriptorSetLayoutKeyHash> s_LayoutCache;
};