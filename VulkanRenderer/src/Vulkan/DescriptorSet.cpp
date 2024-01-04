#include "DescriptorSet.h"

#include <algorithm>

#include "Driver.h"
#include "RenderCommand.h"
#include "VulkanCore.h"

DescriptorSetLayout DescriptorSetLayout::Builder::Build()
{
    PreBuild();
    
    DescriptorSetLayout layout = DescriptorSetLayout::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([layout](){ DescriptorSetLayout::Destroy(layout); });
    
    return layout;
}

DescriptorSetLayout DescriptorSetLayout::Builder::BuildManualLifetime()
{
    PreBuild();
    
    return DescriptorSetLayout::Create(m_CreateInfo);
}

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::SetBindings(
    const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
    m_CreateInfo.Bindings = bindings;

    return *this;
}

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::SetBindingFlags(const std::vector<VkDescriptorBindingFlags>& flags)
{
    m_CreateInfo.BindingFlags = flags;

    return *this;
}

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::SetFlags(VkDescriptorSetLayoutCreateFlags flags)
{
    m_CreateInfo.Flags = flags;

    return *this;
}

void DescriptorSetLayout::Builder::PreBuild()
{
    if (m_CreateInfo.BindingFlags.empty())
        m_CreateInfo.BindingFlags.resize(m_CreateInfo.Bindings.size());
    ASSERT(m_CreateInfo.BindingFlags.size() == m_CreateInfo.Bindings.size(),
        "Is any element of binding flags is set, every element have to be set")
}

DescriptorSetLayout DescriptorSetLayout::Create(const Builder::CreateInfo& createInfo)
{
    DescriptorSetLayout layout = {};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.bindingCount = (u32)createInfo.BindingFlags.size();
    bindingFlags.pBindingFlags = createInfo.BindingFlags.data();
    
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = (u32)createInfo.Bindings.size();
    layoutCreateInfo.pBindings = createInfo.Bindings.data();
    layoutCreateInfo.flags = createInfo.Flags;
    layoutCreateInfo.pNext = &bindingFlags;

    VulkanCheck(vkCreateDescriptorSetLayout(Driver::DeviceHandle(), &layoutCreateInfo, nullptr, &layout.m_Layout),
        "Failed to create descriptor set layout");

    layout.m_Descriptors.resize(createInfo.Bindings.size());
    for (u32 i = 0; i < layout.m_Descriptors.size(); i++)
        layout.m_Descriptors[i] = createInfo.Bindings[i].descriptorType;

    return layout;
}

void DescriptorSetLayout::Destroy(const DescriptorSetLayout& layout)
{
    vkDestroyDescriptorSetLayout(Driver::DeviceHandle(), layout.m_Layout, nullptr);
}

DescriptorSet DescriptorSet::Builder::Build()
{
    PreBuild();
    
    DescriptorSet set = DescriptorSet::Create(m_CreateInfo);
    m_CreateInfo.BoundResources.clear();
    
    return set;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetAllocator(DescriptorAllocator* allocator)
{
    m_CreateInfo.Allocator = allocator;

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetLayout(const DescriptorSetLayout* layout)
{
    m_CreateInfo.Layout = layout;

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetPoolFlags(VkDescriptorPoolCreateFlags flags)
{
    m_CreateInfo.PoolFlags |= flags;

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddBufferBinding(u32 slot, const BufferBindingInfo& bindingInfo, VkDescriptorType descriptor)
{
    Driver::DescriptorSetBindBuffer(slot, bindingInfo, descriptor, m_CreateInfo);
    
    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddTextureBinding(u32 slot, const Texture& texture, VkDescriptorType descriptor)
{
    Driver::DescriptorSetBindTexture(slot, texture, descriptor, m_CreateInfo);
    
    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddTextureBinding(u32 slot, const TextureBindingInfo& texture,
    VkDescriptorType descriptor)
{
    Driver::DescriptorSetBindTexture(slot, texture, descriptor, m_CreateInfo);
    
    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddVariableBinding(const VariableBindingInfo& variableBindingInfo)
{
    m_VariableBindingSlots.push_back(variableBindingInfo.Slot);
    m_VariableBindingCounts.push_back(variableBindingInfo.Count);

    return *this;
}

void DescriptorSet::Builder::PreBuild()
{
    std::vector<VariableBindingInfo> variableBindingInfos(m_VariableBindingSlots.size());
    for (u32 i = 0; i < variableBindingInfos.size(); i++)
        variableBindingInfos[i] = {.Slot = m_VariableBindingSlots[i], .Count = m_VariableBindingCounts[i]};
    std::ranges::sort(variableBindingInfos,
        [](u32 a, u32 b) { return a < b; },
        [](const VariableBindingInfo& v) { return v.Slot; });

    for (u32 i = 0; i < variableBindingInfos.size(); i++)
        m_VariableBindingCounts[i] = variableBindingInfos[i].Count;
    
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCount = {};
    variableDescriptorCount.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCount.descriptorSetCount = (u32)m_VariableBindingCounts.size();
    variableDescriptorCount.pDescriptorCounts = m_VariableBindingCounts.data();

    m_CreateInfo.VariableDescriptorCounts = variableDescriptorCount;    
}

DescriptorSet DescriptorSet::Create(const Builder::CreateInfo& createInfo)
{
    DescriptorSet descriptorSet = {};

    descriptorSet.m_Layout = createInfo.Layout;

    createInfo.Allocator->Allocate(descriptorSet, createInfo.PoolFlags, createInfo.VariableDescriptorCounts);

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(createInfo.BoundResources.size());
    for (u32 i = 0; i < createInfo.BoundResources.size(); i++)
    {
        auto& resource = createInfo.BoundResources[i];
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorCount = 1;   
        write.dstSet = descriptorSet.m_DescriptorSet;
        write.descriptorType = resource.Type;
        u32 slot = resource.Slot;
        write.dstBinding = slot;
        if (resource.Buffer.has_value())
            write.pBufferInfo = resource.Buffer.operator->();
        else
            write.pImageInfo = resource.Texture.operator->();

        writes.push_back(write);
    }

    vkUpdateDescriptorSets(Driver::DeviceHandle(), (u32)writes.size(), writes.data(), 0, nullptr);

    return descriptorSet;
}

void DescriptorSet::Destroy(const DescriptorSet& descriptorSet)
{
    vkFreeDescriptorSets(Driver::DeviceHandle(), descriptorSet.m_Pool, 1, &descriptorSet.m_DescriptorSet);
}

void DescriptorSet::Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint)
{
    RenderCommand::BindDescriptorSet(commandBuffer, *this, pipelineLayout, setIndex, bindPoint, {});
}

void DescriptorSet::Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint,
    const std::vector<u32>& dynamicOffsets)
{
    RenderCommand::BindDescriptorSet(commandBuffer, *this, pipelineLayout, setIndex, bindPoint, dynamicOffsets);
}

void DescriptorSet::SetTexture(u32 slot, const Texture& texture, VkDescriptorType descriptor, u32 arrayIndex)
{
    TextureDescriptorInfo descriptorInfo = texture.CreateDescriptorInfo();
    VkDescriptorImageInfo descriptorTextureInfo = {};
    descriptorTextureInfo.sampler = descriptorInfo.Sampler;
    descriptorTextureInfo.imageLayout = descriptorInfo.Layout;
    descriptorTextureInfo.imageView = descriptorInfo.View;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;   
    write.dstSet = m_DescriptorSet;
    write.descriptorType = descriptor;
    write.dstBinding = slot;
    write.pImageInfo = &descriptorTextureInfo;
    write.dstArrayElement = arrayIndex;

    vkUpdateDescriptorSets(Driver::DeviceHandle(), 1, &write, 0, nullptr);
}

DescriptorAllocator DescriptorAllocator::Builder::Build()
{
    return DescriptorAllocator::Create(m_CreateInfo);
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
    DescriptorAllocator allocator = {};
    allocator.m_MaxSetsPerPool = createInfo.MaxSets;

    return allocator;
}

void DescriptorAllocator::Allocate(DescriptorSet& set, VkDescriptorPoolCreateFlags poolFlags,
        const VkDescriptorSetVariableDescriptorCountAllocateInfo& variableDescriptorCounts)
{
    SetAllocateInfo allocateInfo = {};
    allocateInfo.Info.pNext = &variableDescriptorCounts;

    u32 poolIndex = GrabPool(poolFlags);
    PoolInfo pool = m_FreePools[poolIndex];
    Driver::Unpack(pool, *set.GetLayout(), allocateInfo);

    vkAllocateDescriptorSets(Driver::DeviceHandle(), &allocateInfo.Info, &set.m_DescriptorSet);
    set.m_Pool = pool.Pool;

    if (!set.IsValid())
    {
        m_UsedPools.push_back(m_FreePools[poolIndex]);
        m_FreePools.erase(m_FreePools.begin() + poolIndex);

        poolIndex = GrabPool(poolFlags);
        pool = m_FreePools[poolIndex];
        Driver::Unpack(pool, *set.GetLayout(), allocateInfo);
        VulkanCheck(vkAllocateDescriptorSets(Driver::DeviceHandle(), &allocateInfo.Info, &set.m_DescriptorSet),
            "Failed to allocate descriptor set");
        set.m_Pool = pool.Pool;
    }
}

void DescriptorAllocator::ResetPools()
{
    for (auto pool : m_UsedPools)
    {
        vkResetDescriptorPool(Driver::DeviceHandle(), pool.Pool, 0);
        m_FreePools.push_back(pool);
    }

    m_UsedPools.clear();
}

u32 DescriptorAllocator::GrabPool(VkDescriptorPoolCreateFlags poolFlags)
{
    for (u32 i = 0; i < m_FreePools.size(); i++)
        if (m_FreePools[i].Flags == poolFlags)
            return i;

    u32 index = (u32)m_FreePools.size();
    m_FreePools.push_back(CreatePool(poolFlags));

    return index;
}

DescriptorAllocator::PoolInfo DescriptorAllocator::CreatePool(VkDescriptorPoolCreateFlags poolFlags)
{
    std::vector<VkDescriptorPoolSize> sizes(m_PoolSizes.size());
    for (u32 i = 0; i < sizes.size(); i++)
        sizes[i] = {.type = m_PoolSizes[i].DescriptorType, .descriptorCount = (u32)(m_PoolSizes[i].SetSizeMultiplier * (f32)m_MaxSetsPerPool) };

    VkDescriptorPool pool = {};
    
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = m_MaxSetsPerPool;
    poolCreateInfo.poolSizeCount = (u32)sizes.size();
    poolCreateInfo.pPoolSizes = sizes.data();
    poolCreateInfo.flags = poolFlags;

    VulkanCheck(vkCreateDescriptorPool(Driver::DeviceHandle(), &poolCreateInfo, nullptr, &pool),
        "Failed to create descriptor pool");

    Driver::DeletionQueue().AddDeleter([pool]() { vkDestroyDescriptorPool(Driver::DeviceHandle(), pool, nullptr); });
    
    return {.Pool = pool, .Flags = poolFlags};
}

bool DescriptorLayoutCache::CacheKey::operator==(const CacheKey& other) const
{
    if (Flags != other.Flags)
        return false;
    
    if (Bindings.size() != other.Bindings.size())
        return false;

    for (u32 i = 0; i < Bindings.size(); i++)
    {
        if (Bindings[i].binding != other.Bindings[i].binding)
            return false;
        if (Bindings[i].descriptorType != other.Bindings[i].descriptorType)
            return false;
        if (Bindings[i].descriptorCount != other.Bindings[i].descriptorCount)
            return false;
        if (Bindings[i].stageFlags != other.Bindings[i].stageFlags)
            return false;
        
        if (BindingFlags[i] != other.BindingFlags[i])
            return false;
    }
    
    return true;
}

DescriptorSetLayout* DescriptorLayoutCache::CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings,
        const std::vector<VkDescriptorBindingFlags>& bindingFlags, VkDescriptorSetLayoutCreateFlags layoutFlags)
{
    CacheKey key = {.Bindings = bindings, .BindingFlags = bindingFlags, .Flags = layoutFlags};
    SortBindings(key);

    if (m_LayoutCache.contains(key))
        return &m_LayoutCache.at(key);

    m_LayoutCache.emplace(key, DescriptorSetLayout::Builder()
        .SetBindings(bindings)
        .SetBindingFlags(bindingFlags)
        .SetFlags(layoutFlags)
        .Build());
    return &m_LayoutCache.at(key);
}

void DescriptorLayoutCache::SortBindings(CacheKey& cacheKey)
{
    std::sort(cacheKey.Bindings.begin(), cacheKey.Bindings.end(),
        [](const auto& a, const auto& b) { return a.binding < b.binding; });
}

u64 DescriptorLayoutCache::DescriptorSetLayoutCreateInfoHash::operator()(const CacheKey& cacheKey) const
{
    u64 hash = 0;
    for (auto& binding : cacheKey.Bindings)
    {
        u64 hashKey = binding.binding | binding.descriptorCount << 8 | binding.descriptorType << 16 | binding.stageFlags << 24;
        hash ^= std::hash<u64>()(hashKey);
    }
    for (auto& bindingFlag : cacheKey.BindingFlags)
        hash ^= std::hash<u64>()(bindingFlag);

    hash ^= std::hash<u64>()(cacheKey.Flags);
    
    return hash;
}
