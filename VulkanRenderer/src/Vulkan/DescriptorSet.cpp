#include "DescriptorSet.h"

#include "Driver.h"
#include "RenderCommand.h"

DescriptorSetLayout DescriptorSetLayout::Builder::Build()
{
    DescriptorSetLayout layout = DescriptorSetLayout::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([layout](){ DescriptorSetLayout::Destroy(layout); });
    
    return layout;
}

DescriptorSetLayout DescriptorSetLayout::Builder::BuildManualLifetime()
{
    return DescriptorSetLayout::Create(m_CreateInfo);
}

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::SetBindings(
    const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
    m_CreateInfo.Bindings = bindings;

    return *this;
}

DescriptorSetLayout DescriptorSetLayout::Create(const Builder::CreateInfo& createInfo)
{
    DescriptorSetLayout layout = {};
    
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = (u32)createInfo.Bindings.size();
    layoutCreateInfo.pBindings = createInfo.Bindings.data();

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
    DescriptorSet set = DescriptorSet::Create(m_CreateInfo);
    m_CreateInfo.Bindings.clear();
    m_CreateInfo.BoundResources.clear();
    
    return set;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetAllocator(DescriptorAllocator* allocator)
{
    m_CreateInfo.Allocator = allocator;

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetLayoutCache(DescriptorLayoutCache* cache)
{
    m_CreateInfo.Cache = cache;

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddBufferBinding(u32 slot, const BufferBindingInfo& bindingInfo,
    VkDescriptorType descriptor, VkShaderStageFlags stages)
{
    Driver::DescriptorSetBindBuffer(slot, bindingInfo, descriptor, stages, m_CreateInfo);

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::AddTextureBinding(u32 slot, const Texture& texture,
    VkDescriptorType descriptor, VkShaderStageFlags stages)
{
    Driver::DescriptorSetBindTexture(slot, texture, descriptor, stages, m_CreateInfo);

    return *this;
}

DescriptorSet DescriptorSet::Create(const Builder::CreateInfo& createInfo)
{
    DescriptorSet descriptorSet = {};

    descriptorSet.m_Layout = createInfo.Cache->CreateDescriptorSetLayout(createInfo.Bindings);

    createInfo.Allocator->Allocate(descriptorSet);

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(createInfo.BoundResources.size());
    for (u32 i = 0; i < createInfo.BoundResources.size(); i++)
    {
        auto& resource = createInfo.BoundResources[i];
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorCount = 1;   
        write.dstSet = descriptorSet.m_DescriptorSet;
        write.descriptorType = createInfo.Bindings[i].descriptorType;
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

void DescriptorSet::Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint)
{
    RenderCommand::BindDescriptorSet(commandBuffer, *this, pipelineLayout, setIndex, bindPoint, {});
}

void DescriptorSet::Bind(const CommandBuffer& commandBuffer, const PipelineLayout& pipelineLayout, u32 setIndex, VkPipelineBindPoint bindPoint,
    const std::vector<u32>& dynamicOffsets)
{
    RenderCommand::BindDescriptorSet(commandBuffer, *this, pipelineLayout, setIndex, bindPoint, dynamicOffsets);
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

void DescriptorAllocator::Allocate(DescriptorSet& set)
{
    SetAllocateInfo allocateInfo = {};

    Driver::Unpack(*this, *set.GetLayout(), allocateInfo);

    vkAllocateDescriptorSets(Driver::DeviceHandle(), &allocateInfo.Info, &set.m_DescriptorSet);

    if (!set.IsValid())
    {
        m_UsedPools.push_back(m_FreePools.back());
        m_FreePools.pop_back();

        Driver::Unpack(*this, *set.GetLayout(), allocateInfo);
        VulkanCheck(vkAllocateDescriptorSets(Driver::DeviceHandle(), &allocateInfo.Info, &set.m_DescriptorSet),
            "Failed to allocate descriptor set");
    }
}

void DescriptorAllocator::ResetPools()
{
    for (auto pool : m_UsedPools)
    {
        vkResetDescriptorPool(Driver::DeviceHandle(), pool, 0);
        m_FreePools.push_back(pool);
    }

    m_UsedPools.clear();
}

VkDescriptorPool DescriptorAllocator::GrabPool()
{
    if (m_FreePools.empty())
        m_FreePools.push_back(CreatePool());

    return m_FreePools.back();
}

VkDescriptorPool DescriptorAllocator::CreatePool()
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

    VulkanCheck(vkCreateDescriptorPool(Driver::DeviceHandle(), &poolCreateInfo, nullptr, &pool),
        "Failed to create descriptor pool");

    Driver::DeletionQueue().AddDeleter([pool]() { vkDestroyDescriptorPool(Driver::DeviceHandle(), pool, nullptr); });
    
    return pool;
}

bool DescriptorLayoutCache::CacheKey::operator==(const CacheKey& other) const
{
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
    }
    
    return true;
}

DescriptorSetLayout* DescriptorLayoutCache::CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
    CacheKey key = {.Bindings = bindings};
    SortBindings(key);

    if (m_LayoutCache.contains(key))
        return &m_LayoutCache.at(key);

    m_LayoutCache.emplace(key, DescriptorSetLayout::Builder().SetBindings(bindings).Build());
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
    return hash;
}
