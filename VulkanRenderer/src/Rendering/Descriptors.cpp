#include "Descriptors.h"

#include <algorithm>

#include "Vulkan/Driver.h"
#include "Vulkan/RenderCommand.h"

DescriptorsLayout DescriptorsLayout::Builder::Build()
{
    PreBuild();

    return DescriptorLayoutCache::CreateDescriptorSetLayout(m_CreateInfo);
}

DescriptorsLayout::Builder& DescriptorsLayout::Builder::SetBindings(
    const std::vector<DescriptorBinding>& bindings)
{
    m_CreateInfo.Bindings = bindings;

    return *this;
}

DescriptorsLayout::Builder& DescriptorsLayout::Builder::SetBindingFlags(const std::vector<DescriptorFlags>& flags)
{
    m_CreateInfo.BindingFlags = flags;

    return *this;
}

DescriptorsLayout::Builder& DescriptorsLayout::Builder::SetFlags(DescriptorLayoutFlags flags)
{
    m_CreateInfo.Flags |= flags;

    return *this;
}

void DescriptorsLayout::Builder::PreBuild()
{
    if (m_CreateInfo.BindingFlags.empty())
        m_CreateInfo.BindingFlags.resize(m_CreateInfo.Bindings.size());
    ASSERT(m_CreateInfo.BindingFlags.size() == m_CreateInfo.Bindings.size(),
        "If any element of binding flags is set, every element has to be set")
}

DescriptorsLayout DescriptorsLayout::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void DescriptorsLayout::Destroy(const DescriptorsLayout& layout)
{
    Driver::Destroy(layout.Handle());
}

DescriptorSet DescriptorSet::Builder::Build()
{
    DescriptorSet set = DescriptorSet::Create(m_CreateInfo);
    m_CreateInfo.Buffers.clear();
    m_CreateInfo.Textures.clear();

    return set;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetAllocator(DescriptorAllocator* allocator)
{
    m_CreateInfo.Allocator = allocator;

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetLayout(DescriptorsLayout layout)
{
    m_CreateInfo.Layout = layout;

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetPoolFlags(DescriptorPoolFlags flags)
{
    m_CreateInfo.PoolFlags |= flags;
    
    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddBufferBinding(u32 slot, const BufferBindingInfo& bindingInfo,
    DescriptorType descriptor)
{
    m_CreateInfo.Buffers.push_back({
        .BindingInfo = bindingInfo,
        .Slot = slot,
        .Type = descriptor});
    
    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddTextureBinding(u32 slot, const TextureBindingInfo& texture,
    DescriptorType descriptor)
{
    m_CreateInfo.Textures.push_back({
        .BindingInfo = texture,
        .Slot = slot,
        .Type = descriptor});
    
    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddVariableBinding(const VariableBindingInfo& variableBindingInfo)
{
    m_CreateInfo.VariableBindingSlots.push_back(variableBindingInfo.Slot);
    m_CreateInfo.VariableBindingCounts.push_back(variableBindingInfo.Count);

    return *this;
}

DescriptorSet DescriptorSet::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

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
    Driver::UpdateDescriptorSet(*this, slot, texture, descriptor, arrayIndex);
}

DescriptorAllocator DescriptorAllocator::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

DescriptorAllocator DescriptorAllocator::Builder::Build(DeletionQueue& deletionQueue)
{
    DescriptorAllocator allocator = DescriptorAllocator::Create(m_CreateInfo);
    deletionQueue.Enqueue(allocator);

    return allocator;
}

DescriptorAllocator DescriptorAllocator::Builder::BuildManualLifetime()
{
    return DescriptorAllocator::Create(m_CreateInfo);
}

DescriptorAllocator::Builder& DescriptorAllocator::Builder::SetMaxSetsPerPool(u32 maxSets)
{
    m_CreateInfo.MaxSets = maxSets;

    return *this;
}

DescriptorAllocator DescriptorAllocator::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void DescriptorAllocator::Destroy(const DescriptorAllocator& allocator)
{
    Driver::Destroy(allocator.Handle());
}

void DescriptorAllocator::Allocate(DescriptorSet& set, DescriptorPoolFlags poolFlags,
    const std::vector<u32>& variableBindingCounts)
{
    return Driver::AllocateDescriptorSet(*this, set, poolFlags, variableBindingCounts);
}

void DescriptorAllocator::Deallocate(ResourceHandle<DescriptorSet> set)
{
    Driver::DeallocateDescriptorSet(Handle(), set);
}

void DescriptorAllocator::ResetPools()
{
    Driver::ResetAllocator(*this);
}

void Descriptors::UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const
{
    Driver::UpdateDescriptors(*this, bindingInfo.Slot, buffer, bindingInfo.Type);
}

void Descriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const
{
    Driver::UpdateDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, 0);
}

void Descriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
    u32 bindlessIndex) const
{
    Driver::UpdateDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, bindlessIndex);
}

void Descriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const
{
    Driver::UpdateGlobalDescriptors(*this, bindingInfo.Slot, buffer, bindingInfo.Type);
}

void Descriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const
{
    Driver::UpdateGlobalDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, 0);
}

void Descriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
    u32 bindlessIndex) const
{
    Driver::UpdateGlobalDescriptors(*this, bindingInfo.Slot, texture, bindingInfo.Type, bindlessIndex);
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
    return Build(Driver::DeletionQueue());
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
    return Driver::Create(createInfo);
}

Descriptors DescriptorArenaAllocator::Allocate(DescriptorsLayout layout,
    const DescriptorAllocatorAllocationBindings& bindings)
{
    ASSERT(m_Residence == DescriptorAllocatorResidence::CPU, "GPU allocators need ResourceUploader to be provided")
    ValidateBindings(bindings);

    std::optional<Descriptors> descriptors = Driver::Allocate(*this, layout, bindings);
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
    if (CreateInfo.Flags != other.CreateInfo.Flags)
        return false;
    
    if (CreateInfo.Bindings.size() != other.CreateInfo.Bindings.size())
        return false;

    for (u32 i = 0; i < CreateInfo.Bindings.size(); i++)
    {
        if (CreateInfo.Bindings[i].Binding != other.CreateInfo.Bindings[i].Binding)
            return false;
        if (CreateInfo.Bindings[i].Type != other.CreateInfo.Bindings[i].Type)
            return false;
        if (CreateInfo.Bindings[i].Count != other.CreateInfo.Bindings[i].Count)
            return false;
        if (CreateInfo.Bindings[i].Shaders != other.CreateInfo.Bindings[i].Shaders)
            return false;
        if (CreateInfo.Bindings[i].DescriptorFlags != other.CreateInfo.Bindings[i].DescriptorFlags)
            return false;
        
        if (CreateInfo.BindingFlags[i] != other.CreateInfo.BindingFlags[i])
            return false;
    }
    
    return true;
}

DescriptorsLayout DescriptorLayoutCache::CreateDescriptorSetLayout(
    const DescriptorsLayout::Builder::CreateInfo& createInfo)
{
    CacheKey key = {.CreateInfo = createInfo};
    SortBindings(key);

    if (s_LayoutCache.contains(key))
        return s_LayoutCache.at(key);

    DescriptorsLayout newLayout = DescriptorsLayout::Create(createInfo);
    s_LayoutCache.emplace(key, newLayout);

    Driver::DeletionQueue().Enqueue(newLayout);
    
    return newLayout;
}

void DescriptorLayoutCache::SortBindings(CacheKey& cacheKey)
{
    std::sort(cacheKey.CreateInfo.Bindings.begin(), cacheKey.CreateInfo.Bindings.end(),
        [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
}

u64 DescriptorLayoutCache::DescriptorSetLayoutKeyHash::operator()(const CacheKey& cacheKey) const
{
    u64 hash = 0;
    for (auto& binding : cacheKey.CreateInfo.Bindings)
    {
        u64 hashKey = binding.Binding | binding.Count << 8 | (u32)binding.Type << 16 | (u32)binding.Shaders << 24;
        hash ^= std::hash<u64>()(hashKey);
    }
    for (auto& bindingFlag : cacheKey.CreateInfo.BindingFlags)
        hash ^= std::hash<u64>()((u64)bindingFlag);

    hash ^= std::hash<u64>()((u64)cacheKey.CreateInfo.Flags);
    
    return hash;
}
