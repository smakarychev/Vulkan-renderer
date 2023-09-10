#include "DescriptorSet.h"

#include "Driver.h"
#include "RenderCommand.h"

DescriptorPool DescriptorPool::Builder::Build()
{
    DescriptorPool pool = DescriptorPool::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([pool](){ DescriptorPool::Destroy(pool); });

    return pool;
}

DescriptorPool DescriptorPool::Builder::BuildManualLifetime()
{
    return DescriptorPool::Create(m_CreateInfo);
}

DescriptorPool::Builder& DescriptorPool::Builder::Defaults()
{
    m_CreateInfo.Sizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
    };
    m_CreateInfo.MaxSets = 10;

    return *this;
}

DescriptorPool DescriptorPool::Create(const Builder::CreateInfo& createInfo)
{
    DescriptorPool descriptorPool = {};

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.maxSets = createInfo.MaxSets;
    poolCreateInfo.poolSizeCount = (u32)createInfo.Sizes.size();
    poolCreateInfo.pPoolSizes = createInfo.Sizes.data();

    VulkanCheck(vkCreateDescriptorPool(Driver::DeviceHandle(), &poolCreateInfo, nullptr, &descriptorPool.m_Pool),
        "Failed to create descriptor pool");

    return descriptorPool;
}

void DescriptorPool::Destroy(const DescriptorPool& pool)
{
    vkDestroyDescriptorPool(Driver::DeviceHandle(), pool.m_Pool, nullptr);
}

DescriptorSet DescriptorPool::Allocate(const DescriptorSetLayout& layout)
{
    DescriptorSet descriptorSet = DescriptorSet::Builder().
        SetPool(*this).
        SetLayout(layout).
        Build();

    return descriptorSet;
}

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

DescriptorSetLayout::Builder& DescriptorSetLayout::Builder::AddBinding(VkDescriptorType type, VkShaderStageFlags stage)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = (u32)m_CreateInfo.Bindings.size();
    binding.descriptorCount = 1;
    binding.descriptorType = type;
    binding.stageFlags = stage;

    m_CreateInfo.Bindings.push_back(binding);

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
    return DescriptorSet::Create(m_CreateInfo);
}

DescriptorSet::Builder& DescriptorSet::Builder::SetPool(const DescriptorPool& pool)
{
    Driver::Unpack(pool, m_CreateInfo);

    return *this;
}

DescriptorSet::Builder& DescriptorSet::Builder::SetLayout(const DescriptorSetLayout& layout)
{
    Driver::Unpack(layout, m_CreateInfo);
    m_CreateInfo.Layout = &layout;

    return *this;
}

DescriptorSet DescriptorSet::Create(const Builder::CreateInfo& createInfo)
{
    DescriptorSet descriptorSet = {};
    
    VkDescriptorSetAllocateInfo setAllocateInfo = {};
    setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocateInfo.descriptorPool = createInfo.Pool;
    setAllocateInfo.descriptorSetCount = 1;
    setAllocateInfo.pSetLayouts = &createInfo.LayoutHandle;

    VulkanCheck(vkAllocateDescriptorSets(Driver::DeviceHandle(), &setAllocateInfo, &descriptorSet.m_DescriptorSet),
        "Failed to allocate descriptor set");
    descriptorSet.m_Layout = createInfo.Layout;

    return descriptorSet;
}

void DescriptorSet::BindBuffer(u32 slot, const Buffer& buffer, u64 sizeBytes)
{
    Driver::DescriptorSetBindBuffer(*this, slot, buffer, sizeBytes, 0);
}

void DescriptorSet::BindBuffer(u32 slot, const Buffer& buffer, u64 sizeBytes, u64 offsetBytes)
{
    Driver::DescriptorSetBindBuffer(*this, slot, buffer, sizeBytes, offsetBytes);
}

void DescriptorSet::BindTexture(u32 slot, const Texture& texture)
{
    Driver::DescriptorSetBindTexture(*this, slot, texture);
}

void DescriptorSet::Bind(const CommandBuffer& commandBuffer, const Pipeline& pipeline, VkPipelineBindPoint bindPoint)
{
    RenderCommand::BindDescriptorSet(commandBuffer, *this, pipeline, bindPoint, {});
}

void DescriptorSet::Bind(const CommandBuffer& commandBuffer, const Pipeline& pipeline, VkPipelineBindPoint bindPoint,
    const std::vector<u32>& dynamicOffsets)
{
    RenderCommand::BindDescriptorSet(commandBuffer, *this, pipeline, bindPoint, dynamicOffsets);
}
