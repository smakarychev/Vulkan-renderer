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

DescriptorArenaAllocator DescriptorArenaAllocator::Builder::Build()
{
    return Build(Device::DeletionQueue());
}

DescriptorArenaAllocator DescriptorArenaAllocator::Builder::Build(DeletionQueue& deletionQueue)
{
    PreBuild();
    
    DescriptorArenaAllocator allocator = DescriptorArenaAllocator::Create(m_CreateInfo);
    for (auto& buffer : allocator.m_Buffers)
        deletionQueue.Enqueue(buffer);

    return allocator;
}

DescriptorArenaAllocator::Builder& DescriptorArenaAllocator::Builder::Kind(DescriptorAllocatorKind kind)
{
    m_CreateInfo.Kind = kind;

    return *this;
}

DescriptorArenaAllocator::Builder& DescriptorArenaAllocator::Builder::Residence(DescriptorAllocatorResidence residence)
{
    m_CreateInfo.Residence = residence;

    return *this;
}

DescriptorArenaAllocator::Builder& DescriptorArenaAllocator::Builder::Count(u32 count)
{
    m_CreateInfo.DescriptorCount = count;

    return *this;
}

DescriptorArenaAllocator::Builder& DescriptorArenaAllocator::Builder::ForTypes(const std::vector<DescriptorType>& types)
{
    m_CreateInfo.UsedTypes = types;

    return *this;
}

void DescriptorArenaAllocator::Builder::PreBuild()
{
    ASSERT(!m_CreateInfo.UsedTypes.empty(), "At least one descriptor type is necessary")
    
    if (m_CreateInfo.Kind == DescriptorAllocatorKind::Resources)
        for (auto type : m_CreateInfo.UsedTypes)
            ASSERT(type != DescriptorType::Sampler,
                "Cannot use allocator of this kind for requested descriptor kinds")
    else
        for (auto type : m_CreateInfo.UsedTypes)
            ASSERT(type == DescriptorType::Sampler,
                "Cannot use allocator of this kind for requested descriptor kinds")
}

DescriptorArenaAllocator DescriptorArenaAllocator::Create(const Builder::CreateInfo& createInfo)
{
    return Device::Create(createInfo);
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


bool DescriptorLayoutCache::CacheKey::operator==(const CacheKey& other) const
{
    if (Flags != other.Flags)
        return false;
    
    if (Bindings.size() != other.Bindings.size())
        return false;

    for (u32 i = 0; i < Bindings.size(); i++)
    {
        if (Bindings[i].Binding != other.Bindings[i].Binding)
            return false;
        if (Bindings[i].Type != other.Bindings[i].Type)
            return false;
        if (Bindings[i].Count != other.Bindings[i].Count)
            return false;
        if (Bindings[i].Shaders != other.Bindings[i].Shaders)
            return false;
        if (Bindings[i].DescriptorFlags != other.Bindings[i].DescriptorFlags)
            return false;
        
        if (BindingFlags[i] != other.BindingFlags[i])
            return false;
    }
    
    return true;
}

DescriptorsLayout DescriptorLayoutCache::CreateDescriptorSetLayout(DescriptorsLayoutCreateInfo&& createInfo)
{
    CacheKey key = {};
    key.Flags = createInfo.Flags;
    key.Bindings.assign(createInfo.Bindings.begin(), createInfo.Bindings.end());
    key.BindingFlags.assign(createInfo.BindingFlags.begin(), createInfo.BindingFlags.end());
    SortBindings(key);

    if (s_LayoutCache.contains(key))
        return s_LayoutCache.at(key);

    DescriptorsLayout newLayout = Device::CreateDescriptorsLayout(std::move(createInfo));
    s_LayoutCache.emplace(key, newLayout);

    Device::DeletionQueue().Enqueue(newLayout);
    
    return newLayout;
}

void DescriptorLayoutCache::SortBindings(CacheKey& cacheKey)
{
    std::sort(cacheKey.Bindings.begin(), cacheKey.Bindings.end(),
        [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
}

u64 DescriptorLayoutCache::DescriptorSetLayoutKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hash = 0;
    for (auto& binding : cacheKey.Bindings)
    {
        u64 hashKey = binding.Binding | binding.Count << 8 | (u32)binding.Type << 16 | (u32)binding.Shaders << 24;
        hash ^= std::hash<u64>()(hashKey);
    }
    for (auto& bindingFlag : cacheKey.BindingFlags)
        hash ^= std::hash<u64>()((u64)bindingFlag);

    hash ^= std::hash<u64>()((u64)cacheKey.Flags);
    
    return hash;
}
