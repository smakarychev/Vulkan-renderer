#include "rendererpch.h"

#include "ShaderPipelineTemplate.h"

#include "core.h"
#include "cvars/CVarSystem.h"
#include "Vulkan/Device.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"

namespace
{
    struct DescriptorsFlags
    {
        std::vector<DescriptorBinding> Descriptors;
        std::vector<DescriptorFlags> Flags;
    };

    DescriptorsFlags extractDescriptorsAndFlags(
        const ShaderReflection::DescriptorsInfo& descriptorSet)
    {
        DescriptorsFlags descriptorsFlags;
        descriptorsFlags.Descriptors.reserve(descriptorSet.Descriptors.size());
        descriptorsFlags.Flags.reserve(descriptorSet.Descriptors.size());
        
        for (auto& descriptor : descriptorSet.Descriptors)
        {
            descriptorsFlags.Descriptors.push_back(descriptor);
            descriptorsFlags.Flags.push_back(descriptor.Flags);
        }

        return descriptorsFlags;
    }

    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> createDescriptorLayouts(
        const std::array<ShaderReflection::DescriptorsInfo, MAX_DESCRIPTOR_SETS>& descriptorSetReflections,
        const std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS>& descriptorLayoutOverrides)
    {
        std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> layouts;

        static const DescriptorsLayout EMPTY_LAYOUT = Device::GetEmptyDescriptorsLayout();

        for (u32 i = 0; i < descriptorSetReflections.size(); i++)
        {
            auto& set = descriptorSetReflections[i];
            if (set.Descriptors.empty())
            {
                layouts[i] = EMPTY_LAYOUT;
                continue;
            }

            if (descriptorLayoutOverrides[i].HasValue())
            {
                layouts[i] = descriptorLayoutOverrides[i];
                continue;
            }
            
            DescriptorsFlags descriptorsFlags = extractDescriptorsAndFlags(set);
        
            DescriptorLayoutFlags layoutFlags = DescriptorLayoutFlags::None;
            if (set.HasBindless)
                layoutFlags |= DescriptorLayoutFlags::UpdateAfterBind;
            
            DescriptorsLayout layout = Device::CreateDescriptorsLayout({
                .Bindings = descriptorsFlags.Descriptors,
                .BindingFlags = descriptorsFlags.Flags,
                .Flags = layoutFlags});
            
            layouts[i] = layout;
        }

        return layouts;
    }
}


ShaderPipelineTemplate::ShaderPipelineTemplate(ShaderPipelineTemplateCreateInfo&& createInfo)
{
    auto& reflection = *createInfo.ShaderReflection;
    m_ShaderReflection = createInfo.ShaderReflection;

    m_DescriptorsLayouts = createDescriptorLayouts(
        reflection.DescriptorSetsInfo(), createInfo.DescriptorLayoutOverrides);
    
    m_PipelineLayout = Device::CreatePipelineLayout({
        .PushConstants = reflection.PushConstants(),
        .DescriptorsLayouts = m_DescriptorsLayouts});

    m_ShaderStageCount = (u32)createInfo.ShaderEntryPoints.size();
    std::ranges::copy(createInfo.ShaderEntryPoints, m_ShaderEntryPoints.begin());
    std::ranges::copy(createInfo.ShaderStages, m_ShaderStages.begin());
}

std::array<bool, MAX_DESCRIPTOR_SETS> ShaderPipelineTemplate::GetSetPresence() const
{
    std::array<bool, MAX_DESCRIPTOR_SETS> presence = {};
    for (u32 setIndex = 0; setIndex < m_ShaderReflection->DescriptorSetsInfo().size(); setIndex++)
        presence[setIndex] = !m_ShaderReflection->DescriptorSetsInfo()[setIndex].Descriptors.empty();

    return presence;
}

bool ShaderPipelineTemplate::IsComputeTemplate() const
{
    return enumHasOnly(m_ShaderReflection->Stages(), ShaderStage::Compute);
}