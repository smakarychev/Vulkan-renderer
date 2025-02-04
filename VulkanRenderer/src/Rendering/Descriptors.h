#pragma once

#include "RenderingCommon.h"

#include "Buffer.h"
#include "DescriptorsTraits.h"
#include "Image/Image.h"
#include "ShaderAsset.h"

#include <array>
#include <vector>
#include <unordered_map>

namespace assetLib
{
    struct ShaderStageInfo;
}

class DescriptorArenaAllocators;
class ResourceUploader;
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

struct DescriptorsLayoutTag{};
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
    struct TextureBinding
    {
        ImageSubresource Subresource{};
        Sampler Sampler{};
        ImageLayout Layout{ImageLayout::Undefined};
    };
    using BoundBuffer = BoundResource<BufferSubresource>;
    using BoundTexture = BoundResource<TextureBinding>;

    DescriptorPoolFlags PoolFlags{0};
    std::span<const BoundBuffer> Buffers;
    std::span<const BoundTexture> Textures;
    DescriptorAllocator Allocator{};
    DescriptorsLayout Layout{};
    std::span<const VariableBindingInfo> VariableBindings;
};

struct DescriptorSetTag{};
using DescriptorSet = ResourceHandleType<DescriptorSetTag>;

struct DescriptorArenaAllocatorTag{};
using DescriptorArenaAllocator = ResourceHandleType<DescriptorArenaAllocatorTag>;

struct DescriptorsTag{};
using Descriptors = ResourceHandleType<DescriptorsTag>;

struct DescriptorBindingInfo
{
    u32 Slot{};
    DescriptorType Type{};
};

enum class DescriptorsKind
{
    Sampler = 0, Resource = 1, Materials = 2,
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
    DescriptorsKind Kind{DescriptorsKind::Resource};
    DescriptorAllocatorResidence Residence{DescriptorAllocatorResidence::CPU};
    Span<const DescriptorType> UsedTypes;
    u32 DescriptorCount{0};
};

class DescriptorArenaAllocators
{
    FRIEND_INTERNAL
public:
    DescriptorArenaAllocators(DescriptorArenaAllocator samplerAllocator, DescriptorArenaAllocator resourceAllocator);
    
    DescriptorArenaAllocator Get(DescriptorsKind kind) const;
private:
    std::array<DescriptorArenaAllocator, (u32)DescriptorsKind::MaxVal> m_Allocators;
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