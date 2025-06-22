#include "ShaderPipelineTemplate.h"

#include <ranges>
#include <algorithm>

#include "AssetManager.h"
#include "core.h"
#include "cvars/CVarSystem.h"
#include "Vulkan/Device.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "Rendering/Commands/RenderCommandList.h"

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
}

DescriptorSlotInfo ShaderPipelineTemplate::GetBinding(u32 set, std::string_view name) const
{
    std::optional<DescriptorSlotInfo> descriptorBinding = TryGetBinding(set, name);
    ASSERT(descriptorBinding.has_value(), "No such binding exists: {}", name)

    return *descriptorBinding;
}

std::optional<DescriptorSlotInfo> ShaderPipelineTemplate::TryGetBinding(u32 set, std::string_view name) const
{
    auto& setInfo = m_ShaderReflection->DescriptorSetsInfo()[set];
    for (u32 descriptorIndex = 0; descriptorIndex < setInfo.Descriptors.size(); descriptorIndex++)
        if (setInfo.DescriptorNames[descriptorIndex] == name)
            return DescriptorSlotInfo{
                .Slot = setInfo.Descriptors[descriptorIndex].Binding,
                .Type = setInfo.Descriptors[descriptorIndex].Type};

    return std::nullopt;
}

std::pair<u32, DescriptorSlotInfo> ShaderPipelineTemplate::GetSetAndBinding(std::string_view name) const
{
    std::optional<DescriptorSlotInfo> descriptorBinding = std::nullopt;
    for (u32 setIndex = 0; setIndex < m_ShaderReflection->DescriptorSetsInfo().size(); setIndex++)
    {
        descriptorBinding = TryGetBinding(setIndex, name);
        if (descriptorBinding.has_value())
            return {setIndex, *descriptorBinding};
    }
    ASSERT(false, "No such binding exists: {}", name)
    std::unreachable();
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

VertexInputDescription ShaderPipelineTemplate::CreateCompatibleVertexDescription(
    const VertexInputDescription& compatibleTo) const
{
    // adapt vertex input layout
    const VertexInputDescription& available = m_ShaderReflection->VertexInputDescription();
    const VertexInputDescription& compatible = compatibleTo;
    ASSERT(available.Bindings.size() <= compatible.Bindings.size(), "Incompatible vertex inputs")
    
    VertexInputDescription adapted;
    adapted.Bindings = compatible.Bindings;
    adapted.Attributes.reserve(compatible.Attributes.size());

    for (u32 availableIndex = 0; availableIndex < available.Attributes.size(); availableIndex++)
    {
        const auto& avail = available.Attributes[availableIndex];
        std::vector<VertexInputDescription::Attribute> candidates;
        candidates.reserve(compatible.Attributes.size());
        for (u32 compatibleIndex = availableIndex; compatibleIndex < compatible.Attributes.size(); compatibleIndex++)
        {
            const auto& comp = compatible.Attributes[compatibleIndex];
            if (comp.Index == avail.Index && comp.Format == avail.Format)
                candidates.push_back(comp);
        }
        for (u32 candidateIndex = 0; candidateIndex < candidates.size(); candidateIndex++)
        {
            const auto& candidate = candidates[candidateIndex];
            if (candidate.BindingIndex == avail.BindingIndex)
            {
                adapted.Attributes.push_back(candidate);
                break;
            }
            if (candidateIndex == candidates.size() - 1)
            {
                LOG("WARNING: compatible attribute found, but binding mismatch detected: expected {} but got {}",
                    avail.BindingIndex, candidate.BindingIndex);
                adapted.Attributes.push_back(candidate);
                break;
            }
        }
        ASSERT(adapted.Attributes.size() == availableIndex + 1, "Incompatible vertex inputs")
    }

    return adapted;
}

std::unordered_map<StringId, ShaderPipelineTemplate> ShaderTemplateLibrary::s_Templates = {};

ShaderPipelineTemplate* ShaderTemplateLibrary::ReloadShaderPipelineTemplate(
    ShaderPipelineTemplateCreateInfo&& createInfo, StringId name)
{
    s_Templates[name] = ShaderPipelineTemplate(std::move(createInfo));
    
    return GetShaderTemplate(name);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(StringId name)
{
    auto it = s_Templates.find(name);
    return it == s_Templates.end() ? nullptr : &it->second;
}