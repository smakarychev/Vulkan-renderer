#include "Descriptors.h"

#include <algorithm>

#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"

void DescriptorSet::BindGraphics(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex)
{
    RenderCommand::BindGraphics(cmd, *this, pipelineLayout, setIndex, {});
}

void DescriptorSet::BindGraphics(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex,
    const std::vector<u32>& dynamicOffsets)
{
    RenderCommand::BindGraphics(cmd, *this, pipelineLayout, setIndex, dynamicOffsets);
}

void DescriptorSet::BindCompute(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex)
{
    RenderCommand::BindCompute(cmd, *this, pipelineLayout, setIndex, {});
}

void DescriptorSet::BindCompute(const CommandBuffer& cmd, PipelineLayout pipelineLayout, u32 setIndex,
    const std::vector<u32>& dynamicOffsets)
{
    RenderCommand::BindCompute(cmd, *this, pipelineLayout, setIndex, dynamicOffsets);
}

void DescriptorSet::SetTexture(u32 slot, const Texture& texture, DescriptorType descriptor, u32 arrayIndex)
{
    Device::UpdateDescriptorSet(*this, slot, texture, descriptor, arrayIndex);
}

void DescriptorAllocator::Allocate(DescriptorSet& set, DescriptorPoolFlags poolFlags,
    const std::vector<u32>& variableBindingCounts)
{
    return Device::AllocateDescriptorSet(*this, set, poolFlags, variableBindingCounts);
}

void DescriptorAllocator::Deallocate(ResourceHandleType<DescriptorSet> set)
{
    Device::DeallocateDescriptorSet(Handle(), set);
}

void DescriptorAllocator::ResetPools()
{
    Device::ResetAllocator(*this);
}

void Descriptors::UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const
{
    Device::UpdateDescriptors(*this, bindingInfo.Slot, buffer, bindingInfo.Type, 0);
}

void Descriptors::UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const
{
    Device::UpdateDescriptors(*this, bindingInfo.Slot, buffer, bindingInfo.Type, index);
}

void Descriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const
{
    Device::UpdateDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, 0);
}

void Descriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture, u32 index) const
{
    Device::UpdateDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, index);
}

void Descriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const
{
    Device::UpdateGlobalDescriptors(*this, bindingInfo.Slot, buffer, bindingInfo.Type, 0);
}

void Descriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const
{
    Device::UpdateGlobalDescriptors(*this, bindingInfo.Slot, buffer, bindingInfo.Type, index);
}

void Descriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const
{
    Device::UpdateGlobalDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, 0);
}

void Descriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
    u32 index) const
{
    Device::UpdateGlobalDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, index);
}

void Descriptors::BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout, u32 firstSet) const
{
    RenderCommand::BindGraphics(cmd, allocators, pipelineLayout, *this, firstSet);
}

void Descriptors::BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout, u32 firstSet) const
{
    RenderCommand::BindCompute(cmd, allocators, pipelineLayout, *this, firstSet);
}

void Descriptors::BindGraphicsImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout,
    u32 firstSet) const
{
    RenderCommand::BindGraphicsImmutableSamplers(cmd, pipelineLayout, firstSet);
}

void Descriptors::BindComputeImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout,
    u32 firstSet) const
{
    RenderCommand::BindComputeImmutableSamplers(cmd, pipelineLayout, firstSet);
}

Descriptors DescriptorArenaAllocator::Allocate(DescriptorsLayout layout,
    const DescriptorAllocatorAllocationBindings& bindings)
{
    ASSERT(m_Residence == DescriptorAllocatorResidence::CPU, "GPU allocators need ResourceUploader to be provided")
    ValidateBindings(bindings);

    std::optional<Descriptors> descriptors = Device::Allocate(*this, layout, bindings);
    ASSERT(descriptors.has_value(), "Increase allocator size")
    
    return *descriptors; 
}

void DescriptorArenaAllocator::Reset()
{
    m_CurrentOffset = 0;
}

void DescriptorArenaAllocator::Bind(const CommandBuffer& cmd, u32 bufferIndex)
{
    m_CurrentBuffer = bufferIndex;

    RenderCommand::Bind(cmd, *this);
}

void DescriptorArenaAllocator::ValidateBindings(const DescriptorAllocatorAllocationBindings& bindings) const
{
    for (auto& binding : bindings.Bindings)
        ASSERT(
            (m_Kind == DescriptorAllocatorKind::Samplers && binding.Type == DescriptorType::Sampler) ||
            (m_Kind == DescriptorAllocatorKind::Resources && binding.Type != DescriptorType::Sampler),
            "Cannot use this descriptor allocator with such bindings")
}

std::unordered_map<DescriptorLayoutCache::CacheKey,
    DescriptorsLayout, DescriptorLayoutCache::DescriptorSetLayoutKeyHash> DescriptorLayoutCache::s_LayoutCache = {};

DescriptorArenaAllocators::DescriptorArenaAllocators(const DescriptorArenaAllocator& resourceAllocator,
    const DescriptorArenaAllocator& samplerAllocator)
    : m_Allocators({resourceAllocator, samplerAllocator})
{
    ASSERT(resourceAllocator.m_Kind == DescriptorAllocatorKind::Resources,
        "Provided 'resource' allocator isn't actually a resource allocator")
    ASSERT(samplerAllocator.m_Kind == DescriptorAllocatorKind::Samplers,
        "Provided 'sampler' allocator isn't actually a sampler allocator")
}

const DescriptorArenaAllocator& DescriptorArenaAllocators::Get(DescriptorAllocatorKind kind) const
{
    ASSERT(kind == DescriptorAllocatorKind::Resources || kind == DescriptorAllocatorKind::Samplers,
           "Unsupported allocator kind")

    return m_Allocators[(u32)kind];
}

DescriptorArenaAllocator& DescriptorArenaAllocators::Get(DescriptorAllocatorKind kind)
{
    return const_cast<DescriptorArenaAllocator&>(const_cast<const DescriptorArenaAllocators&>(*this).Get(kind));
}

void DescriptorArenaAllocators::Bind(const CommandBuffer& cmd, u32 bufferIndex)
{
    for (auto& allocator : m_Allocators)
        allocator.m_CurrentBuffer = bufferIndex;

    RenderCommand::Bind(cmd, *this);
}

DescriptorLayoutCache::CacheKey DescriptorLayoutCache::CreateCacheKey(const DescriptorsLayoutCreateInfo& createInfo)
{
    CacheKey key = {};
    key.m_Flags = createInfo.Flags;
    key.m_Bindings.assign_range(createInfo.Bindings);
    key.m_BindingFlags.assign_range(createInfo.BindingFlags);
    SortBindings(key);

    return key;
}

DescriptorsLayout* DescriptorLayoutCache::Find(const CacheKey& key)
{
    if (s_LayoutCache.contains(key))
        return &s_LayoutCache.at(key);

    return nullptr;
}

void DescriptorLayoutCache::Emplace(const CacheKey& key, DescriptorsLayout layout)
{
    s_LayoutCache.emplace(key, layout);
}

void DescriptorLayoutCache::SortBindings(CacheKey& cacheKey)
{
    std::sort(cacheKey.m_Bindings.begin(), cacheKey.m_Bindings.end(),
        [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
}

u64 DescriptorLayoutCache::DescriptorSetLayoutKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hash = 0;
    for (auto& binding : cacheKey.m_Bindings)
    {
        u64 hashKey = binding.Binding | binding.Count << 8 | (u32)binding.Type << 16 | (u32)binding.Shaders << 24;
        hash ^= std::hash<u64>()(hashKey);
    }
    for (auto& bindingFlag : cacheKey.m_BindingFlags)
        hash ^= std::hash<u64>()((u64)bindingFlag);

    hash ^= std::hash<u64>()((u64)cacheKey.m_Flags);
    
    return hash;
}
