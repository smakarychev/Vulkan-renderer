#include "Shader.h"

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
        const ShaderReflection::DescriptorSetInfo& descriptorSet, bool useDescriptorBuffer)
    {
        DescriptorsFlags descriptorsFlags;
        descriptorsFlags.Descriptors.reserve(descriptorSet.Descriptors.size());
        descriptorsFlags.Flags.reserve(descriptorSet.Descriptors.size());
        
        for (auto& descriptor : descriptorSet.Descriptors)
        {
            descriptorsFlags.Descriptors.push_back(descriptor);
            DescriptorFlags flags = DescriptorFlags::None;
            if (enumHasAny(descriptor.DescriptorFlags, assetLib::ShaderStageInfo::DescriptorSet::Bindless))
            {
                flags |= DescriptorFlags::VariableCount;
                if (!useDescriptorBuffer)
                    flags |= DescriptorFlags::PartiallyBound |
                        DescriptorFlags::UpdateAfterBind |
                        DescriptorFlags::UpdateUnusedPending;
            }
            descriptorsFlags.Flags.push_back(flags);
        }

        return descriptorsFlags;
    }

    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> createDescriptorLayouts(
        const std::array<ShaderReflection::DescriptorSetInfo, MAX_DESCRIPTOR_SETS>& descriptorSetReflections,
        bool useDescriptorBuffer)
    {
        std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> layouts;

        static const DescriptorsLayout EMPTY_LAYOUT_ORDINARY = Device::CreateDescriptorsLayout({}); 
        static const DescriptorsLayout EMPTY_LAYOUT_DESCRIPTOR_BUFFER = Device::CreateDescriptorsLayout({
            .Flags = DescriptorLayoutFlags::DescriptorBuffer});

        const DescriptorsLayout EMPTY_LAYOUT = useDescriptorBuffer ?
            EMPTY_LAYOUT_DESCRIPTOR_BUFFER : EMPTY_LAYOUT_ORDINARY;

        for (u32 i = 0; i < descriptorSetReflections.size(); i++)
        {
            auto& set = descriptorSetReflections[i];
            if (set.Descriptors.empty())
            {
                layouts[i] = EMPTY_LAYOUT;
                continue;
            }
            
            DescriptorsFlags descriptorsFlags = extractDescriptorsAndFlags(set, useDescriptorBuffer);
        
            DescriptorLayoutFlags layoutFlags = set.HasImmutableSampler ?
                DescriptorLayoutFlags::EmbeddedImmutableSamplers : DescriptorLayoutFlags::None;
            if (useDescriptorBuffer)
                layoutFlags |= DescriptorLayoutFlags::DescriptorBuffer;
            else if (set.HasBindless)
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
    ASSERT(
        createInfo.ResourceAllocator.HasValue() ||
        createInfo.SamplerAllocator.HasValue() ||
        createInfo.Allocator.HasValue(),
        "Allocators are unset")
    ASSERT(
        !createInfo.ResourceAllocator.HasValue() && !createInfo.SamplerAllocator.HasValue() ||
        !createInfo.Allocator.HasValue(),
        "Cannot set both allocator and arena allocator")
    
    auto& reflection = *createInfo.ShaderReflection;
    m_ShaderReflection = createInfo.ShaderReflection;

    if (!createInfo.Allocator.HasValue())
    {
        m_UseDescriptorBuffer = true;
        m_Allocator.ResourceAllocator = createInfo.ResourceAllocator;
        m_Allocator.SamplerAllocator = createInfo.SamplerAllocator;
    }
    else
    {
        m_Allocator.DescriptorAllocator = createInfo.Allocator;
    }
    
    m_DescriptorsLayouts = createDescriptorLayouts(
        reflection.DescriptorSetsInfo(),
        m_UseDescriptorBuffer);
    
    m_PipelineLayout = Device::CreatePipelineLayout({
        .PushConstants = reflection.PushConstants(),
        .DescriptorSetLayouts = m_DescriptorsLayouts});
}

DescriptorBindingInfo ShaderPipelineTemplate::GetBinding(u32 set, std::string_view name) const
{
    std::optional<DescriptorBindingInfo> descriptorBinding = TryGetBinding(set, name);
    ASSERT(descriptorBinding.has_value(), "No such binding exists: {}", name)

    return *descriptorBinding;
}

std::optional<DescriptorBindingInfo> ShaderPipelineTemplate::TryGetBinding(u32 set, std::string_view name) const
{
    auto& setInfo = m_ShaderReflection->DescriptorSetsInfo()[set];
    for (u32 descriptorIndex = 0; descriptorIndex < setInfo.Descriptors.size(); descriptorIndex++)
        if (setInfo.DescriptorNames[descriptorIndex] == name)
            return DescriptorBindingInfo{
                .Slot = setInfo.Descriptors[descriptorIndex].Binding,
                .Type = setInfo.Descriptors[descriptorIndex].Type};

    return std::nullopt;
}

std::pair<u32, DescriptorBindingInfo> ShaderPipelineTemplate::GetSetAndBinding(std::string_view name) const
{
    std::optional<DescriptorBindingInfo> descriptorBinding = std::nullopt;
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

DescriptorArenaAllocator ShaderPipelineTemplate::GetAllocator(DescriptorsKind kind) const
{
    return kind == DescriptorsKind::Resource ?
        m_Allocator.ResourceAllocator : m_Allocator.SamplerAllocator;
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

ShaderPipelineTemplate* ShaderTemplateLibrary::LoadShaderPipelineTemplate(const std::vector<std::string>& paths,
    StringId name, DescriptorAllocator allocator)
{
    const StringId templateName = GenerateTemplateName(name, allocator);
    if (!GetShaderTemplate(templateName))
        AddShaderTemplate(CreateFromPaths(paths, allocator), templateName);
    
    return GetShaderTemplate(templateName);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::LoadShaderPipelineTemplate(const std::vector<std::string>& paths,
    StringId name, DescriptorArenaAllocators& allocators)
{
    const StringId templateName = GenerateTemplateName(name, allocators);
    if (!GetShaderTemplate(templateName))
        AddShaderTemplate(CreateFromPaths(paths, allocators), templateName);
    
    return GetShaderTemplate(templateName);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(StringId name,
    DescriptorArenaAllocators& allocators)
{
    return GetShaderTemplate(GenerateTemplateName(name, allocators));
}

ShaderPipelineTemplate* ShaderTemplateLibrary::ReloadShaderPipelineTemplate(const std::vector<std::string>& paths,
    StringId name, DescriptorArenaAllocators& allocators)
{
    StringId templateName = GenerateTemplateName(name, allocators);
    if (s_Templates.contains(templateName))
        s_Templates[templateName] = CreateFromPaths(paths, allocators);
    else
        s_Templates.emplace(templateName, CreateFromPaths(paths, allocators));

    return GetShaderTemplate(templateName);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(StringId name)
{
    auto it = s_Templates.find(name);
    return it == s_Templates.end() ? nullptr : &it->second;
}

StringId ShaderTemplateLibrary::GenerateTemplateName(StringId name, DescriptorAllocator allocator)
{
    return name.Concatenate("_alloc");
}

StringId ShaderTemplateLibrary::GenerateTemplateName(StringId name, DescriptorArenaAllocators& allocators)
{
    return name;
}

void ShaderTemplateLibrary::AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, StringId name)
{
    s_Templates.emplace(std::make_pair(name, shaderTemplate));
}

ShaderPipelineTemplate* ShaderTemplateLibrary::CreateMaterialsTemplate(StringId name,
    DescriptorArenaAllocators& allocators)
{
    return LoadShaderPipelineTemplate(
        {*CVars::Get().GetStringCVar("Path.Shaders.Full"_hsv) + "processed/core/material-frag.stage"},
        name, allocators);
}

ShaderPipelineTemplate ShaderTemplateLibrary::CreateFromPaths(const std::vector<std::string>& paths,
    DescriptorAllocator allocator)
{
    return ShaderPipelineTemplate({
        .ShaderReflection = ShaderReflection::ReflectFrom(paths),
        .Allocator = allocator});
}

ShaderPipelineTemplate ShaderTemplateLibrary::CreateFromPaths(const std::vector<std::string>& paths,
    DescriptorArenaAllocators& allocators)
{
    return ShaderPipelineTemplate({
        .ShaderReflection = ShaderReflection::ReflectFrom(paths),
        .ResourceAllocator = allocators.Get(DescriptorsKind::Resource),
        .SamplerAllocator = allocators.Get(DescriptorsKind::Sampler)});
}
