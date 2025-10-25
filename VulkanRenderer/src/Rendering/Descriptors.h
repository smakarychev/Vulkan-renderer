#pragma once

#include "Buffer/Buffer.h"
#include "DescriptorsTraits.h"
#include "Image/Image.h"

#include <array>
#include <vector>
#include <unordered_map>

// todo: probably 4
static constexpr u32 MAX_DESCRIPTOR_SETS = 3;
static_assert(MAX_DESCRIPTOR_SETS == 3, "Must have exactly 3 sets");

static constexpr u32 BINDLESS_DESCRIPTORS_INDEX = 2;
static_assert(BINDLESS_DESCRIPTORS_INDEX == 2, "Bindless descriptors are expected to be at index 2");

class DescriptorArenaAllocators;
class ResourceUploader;
class DescriptorPool;

struct DescriptorBinding
{
    u32 Binding;
    DescriptorType Type;
    u32 Count;
    ShaderStage Shaders;
    DescriptorFlags Flags{DescriptorFlags::None};
    Sampler ImmutableSampler{};

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

struct DescriptorSlotInfo
{
    u32 Slot{};
    DescriptorType Type{};
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
    u32 DescriptorSet{0};
    DescriptorAllocatorResidence Residence{DescriptorAllocatorResidence::CPU};
    Span<const DescriptorType> UsedTypes;
    u32 DescriptorCount{0};
};

class DescriptorArenaAllocators
{
    FRIEND_INTERNAL
public:
    DescriptorArenaAllocators() = default;
    DescriptorArenaAllocators(Span<const DescriptorArenaAllocator> allocators);
    
    DescriptorArenaAllocator Get(u32 index) const;
    void ResetNonBindless() const;
    void Reset(u32 index) const;
private:
    std::array<DescriptorArenaAllocator, MAX_DESCRIPTOR_SETS> m_Allocators;
    u32 m_AllocatorCount{0};
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