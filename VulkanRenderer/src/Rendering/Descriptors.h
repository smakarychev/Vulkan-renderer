#pragma once

#include "Buffer/Buffer.h"
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

struct DescriptorsLayoutTag{};
using DescriptorsLayout = ResourceHandleType<DescriptorsLayoutTag>;

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
    DescriptorArenaAllocators() = default;
    DescriptorArenaAllocators(
        DescriptorArenaAllocator samplerAllocator,
        DescriptorArenaAllocator resourceAllocator,
        DescriptorArenaAllocator materialAllocator);
    
    DescriptorArenaAllocator Get(DescriptorsKind kind) const;
    void Reset(DescriptorsKind kind) const;
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
    struct DescriptorsLayoutKeyHash
    {
        u64 operator()(const CacheKey& cacheKey) const;
    };
    
    static std::unordered_map<CacheKey, DescriptorsLayout, DescriptorsLayoutKeyHash> s_LayoutCache;
};