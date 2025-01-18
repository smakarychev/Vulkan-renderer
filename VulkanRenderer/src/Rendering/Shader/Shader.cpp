#include "Shader.h"

#include <ranges>
#include <algorithm>

#include "AssetManager.h"
#include "Core/core.h"
#include "Vulkan/Device.h"
#include "Vulkan/RenderCommand.h"
#include "Rendering/Buffer.h"
#include "Rendering/Descriptors.h"
#include "Rendering/Pipeline.h"
#include "utils/utils.h"

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
        createInfo.ResourceAllocator || createInfo.SamplerAllocator || createInfo.Allocator,
        "Allocators are unset")
    ASSERT(
        !createInfo.ResourceAllocator && !createInfo.SamplerAllocator ||
        !createInfo.Allocator,
        "Cannot set both allocator and arena allocator")
    
    auto& reflection = *createInfo.ShaderReflection;
    m_ShaderReflection = createInfo.ShaderReflection;

    if (createInfo.Allocator == nullptr)
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

const DescriptorBinding& ShaderPipelineTemplate::GetBinding(u32 set, std::string_view name) const
{
    const DescriptorBinding* binding = TryGetBinding(set, name);

    ASSERT(binding != nullptr, "Unrecognized descriptor binding name")
    
    return *binding;
}

const DescriptorBinding* ShaderPipelineTemplate::TryGetBinding(u32 set, std::string_view name) const
{
    auto& setInfo = m_ShaderReflection->DescriptorSetsInfo()[set];
    for (u32 descriptorIndex = 0; descriptorIndex < setInfo.Descriptors.size(); descriptorIndex++)
        if (setInfo.DescriptorNames[descriptorIndex] == name)
            return &setInfo.Descriptors[descriptorIndex];

    return nullptr;
}

std::pair<u32, const DescriptorBinding&> ShaderPipelineTemplate::GetSetAndBinding(std::string_view name) const
{
    const DescriptorBinding* descriptorBinding = nullptr;
    u32 set = 0;
    for (u32 setIndex = 0; setIndex < m_ShaderReflection->DescriptorSetsInfo().size(); setIndex++)
    {
        set = setIndex;
        descriptorBinding = TryGetBinding(setIndex, name);
        if (descriptorBinding != nullptr)
            break;
    }
    ASSERT(descriptorBinding != nullptr, "No such binding exists")

    return {set, *descriptorBinding};
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

ShaderDescriptorSet::ShaderDescriptorSet(ShaderDescriptorSetCreateInfo&& createInfo)
{
    ASSERT(!createInfo.ShaderPipelineTemplate->m_UseDescriptorBuffer,
        "ShaderPipelineTemplate was configured to use descriptor buffer, and therefore cannot be used to create"
        " shader descriptor set")
    
    m_Template = createInfo.ShaderPipelineTemplate;

    u32 setCount = 0;
    for (u32 i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        if (!createInfo.DescriptorInfos[i])
            continue;
        
        m_DescriptorSetsInfo.DescriptorSets[i].IsPresent = true;
        m_DescriptorSetsInfo.DescriptorSets[i].Set =
            Device::CreateDescriptorSet(std::move(*createInfo.DescriptorInfos[i]));
        setCount++;
    }
    m_DescriptorSetsInfo.DescriptorCount = setCount;
}

void ShaderDescriptorSet::BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind,
    PipelineLayout pipelineLayout) const
{
    RenderCommand::BindGraphics(cmd, GetDescriptorSet(descriptorKind), pipelineLayout, (u32)descriptorKind, {});
}

void ShaderDescriptorSet::BindGraphics(const CommandBuffer& cmd, DescriptorKind descriptorKind,
    PipelineLayout pipelineLayout, const std::vector<u32>& dynamicOffsets) const
{
    RenderCommand::BindGraphics(cmd, GetDescriptorSet(descriptorKind), pipelineLayout, (u32)descriptorKind,
        dynamicOffsets);
}

void ShaderDescriptorSet::BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind,
    PipelineLayout pipelineLayout) const
{
    RenderCommand::BindCompute(cmd, GetDescriptorSet(descriptorKind), pipelineLayout, (u32)descriptorKind, {});
}

void ShaderDescriptorSet::BindCompute(const CommandBuffer& cmd, DescriptorKind descriptorKind,
                                      PipelineLayout pipelineLayout, const std::vector<u32>& dynamicOffsets) const
{
    RenderCommand::BindCompute(cmd, GetDescriptorSet(descriptorKind), pipelineLayout, (u32)descriptorKind,
        dynamicOffsets);
}

void ShaderDescriptorSet::SetTexture(std::string_view name, const Texture& texture, u32 arrayIndex)
{
    auto&& [set, descriptorBinding] = m_Template->GetSetAndBinding(name);

    ASSERT(m_DescriptorSetsInfo.DescriptorSets[set].IsPresent,
        "Attempt to access non-existing desriptor set")

    Device::UpdateDescriptorSet(m_DescriptorSetsInfo.DescriptorSets[set].Set,
        descriptorBinding.Binding, texture, descriptorBinding.Type, arrayIndex);
}

ShaderDescriptors::ShaderDescriptors(ShaderDescriptorsCreateInfo&& createInfo)
{
    auto* shaderTemplate = createInfo.ShaderPipelineTemplate;

    ASSERT(shaderTemplate->m_UseDescriptorBuffer,
        "Shader pipeline template is not configured to be used with descriptor buffer")

    auto* allocator = createInfo.AllocatorKind == DescriptorAllocatorKind::Resources ?
        shaderTemplate->m_Allocator.ResourceAllocator : 
        shaderTemplate->m_Allocator.SamplerAllocator;
    
    Descriptors descriptors = allocator->Allocate(
        shaderTemplate->GetDescriptorsLayout(createInfo.Set), {
            .Bindings = shaderTemplate->GetReflection().DescriptorSetsInfo()[createInfo.Set].Descriptors,
            .BindlessCount = createInfo.BindlessCount});

    m_Descriptors = descriptors;
    m_SetNumber = createInfo.Set;
    m_Template = createInfo.ShaderPipelineTemplate;
}

void ShaderDescriptors::BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout) const
{
    RenderCommand::BindGraphics(cmd, allocators, pipelineLayout, m_Descriptors, m_SetNumber);
}

void ShaderDescriptors::BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout) const
{
    RenderCommand::BindCompute(cmd, allocators, pipelineLayout, m_Descriptors, m_SetNumber);
}

void ShaderDescriptors::BindGraphicsImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const
{
    RenderCommand::BindGraphicsImmutableSamplers(cmd, pipelineLayout, m_SetNumber);
}

void ShaderDescriptors::BindComputeImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const
{
    RenderCommand::BindComputeImmutableSamplers(cmd, pipelineLayout, m_SetNumber);
}

void ShaderDescriptors::UpdateBinding(std::string_view name, const BufferBindingInfo& buffer) const
{
    m_Descriptors.UpdateBinding(GetBindingInfo(name), buffer);
}

void ShaderDescriptors::UpdateBinding(std::string_view name, const BufferBindingInfo& buffer, u32 index) const
{
    m_Descriptors.UpdateBinding(GetBindingInfo(name), buffer, index);
}

void ShaderDescriptors::UpdateBinding(std::string_view name, const TextureBindingInfo& texture) const
{
    m_Descriptors.UpdateBinding(GetBindingInfo(name), texture);
}

void ShaderDescriptors::UpdateBinding(std::string_view name, const TextureBindingInfo& texture, u32 index) const
{
    m_Descriptors.UpdateBinding(GetBindingInfo(name), texture, index);
}

void ShaderDescriptors::UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const
{
    m_Descriptors.UpdateBinding(bindingInfo, buffer);
}

void ShaderDescriptors::UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer, u32 index) const
{
    m_Descriptors.UpdateBinding(bindingInfo, buffer, index);
}

void ShaderDescriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const
{
    m_Descriptors.UpdateBinding(bindingInfo, texture);
}

void ShaderDescriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
    u32 index) const
{
    m_Descriptors.UpdateBinding(bindingInfo, texture, index);
}

void ShaderDescriptors::UpdateGlobalBinding(std::string_view name, const BufferBindingInfo& buffer) const
{
    m_Descriptors.UpdateGlobalBinding(GetBindingInfo(name), buffer);
}

void ShaderDescriptors::UpdateGlobalBinding(std::string_view name, const BufferBindingInfo& buffer, u32 index) const
{
    m_Descriptors.UpdateGlobalBinding(GetBindingInfo(name), buffer, index);
}

void ShaderDescriptors::UpdateGlobalBinding(std::string_view name, const TextureBindingInfo& texture) const
{
    m_Descriptors.UpdateGlobalBinding(GetBindingInfo(name), texture);
}

void ShaderDescriptors::UpdateGlobalBinding(std::string_view name, const TextureBindingInfo& texture,
    u32 index) const
{
    m_Descriptors.UpdateGlobalBinding(GetBindingInfo(name), texture, index);
}

void ShaderDescriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const
{
    m_Descriptors.UpdateGlobalBinding(bindingInfo, buffer);
}

void ShaderDescriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer,
    u32 index) const
{
    m_Descriptors.UpdateGlobalBinding(bindingInfo, buffer, index);
}

void ShaderDescriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const
{
    m_Descriptors.UpdateGlobalBinding(bindingInfo, texture);
}

void ShaderDescriptors::UpdateGlobalBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
    u32 index) const
{
    m_Descriptors.UpdateGlobalBinding(bindingInfo, texture, index);
}

ShaderDescriptors::BindingInfo ShaderDescriptors::GetBindingInfo(std::string_view bindingName) const
{
    auto& binding = m_Template->GetBinding(m_SetNumber, bindingName);

    return {.Slot = binding.Binding, .Type = binding.Type};
}

std::unordered_map<std::string, ShaderPipelineTemplate> ShaderTemplateLibrary::s_Templates = {};

ShaderPipelineTemplate* ShaderTemplateLibrary::LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
    std::string_view templateName, DescriptorAllocator& allocator)
{
    std::string name = GenerateTemplateName(templateName, allocator);
    if (!GetShaderTemplate(name))
        AddShaderTemplate(CreateFromPaths(paths, allocator), name);
    
    return GetShaderTemplate(name);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
    std::string_view templateName, DescriptorArenaAllocators& allocators)
{
    std::string name = GenerateTemplateName(templateName, allocators);
    if (!GetShaderTemplate(name))
        AddShaderTemplate(CreateFromPaths(paths, allocators), name);
    
    return GetShaderTemplate(name);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(const std::string& name,
    DescriptorArenaAllocators& allocators)
{
    return GetShaderTemplate(GenerateTemplateName(name, allocators));
}

ShaderPipelineTemplate* ShaderTemplateLibrary::ReloadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
    std::string_view templateName, DescriptorArenaAllocators& allocators)
{
    std::string name = GenerateTemplateName(templateName, allocators);
    if (s_Templates.contains(name))
        s_Templates[name] = CreateFromPaths(paths, allocators);
    else
        s_Templates.emplace(name, CreateFromPaths(paths, allocators));

    return GetShaderTemplate(name);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(const std::string& name)
{
    auto it = s_Templates.find(name);
    return it == s_Templates.end() ? nullptr : &it->second;
}

std::string ShaderTemplateLibrary::GenerateTemplateName(std::string_view templateName, DescriptorAllocator& allocator)
{
    return std::string{templateName} + "_alloc";
}

std::string ShaderTemplateLibrary::GenerateTemplateName(std::string_view templateName,
    DescriptorArenaAllocators& allocators)
{
    return std::string{templateName} + "_arena_alloc";
}

void ShaderTemplateLibrary::AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name)
{
    s_Templates.emplace(std::make_pair(name, shaderTemplate));
}

ShaderPipelineTemplate* ShaderTemplateLibrary::CreateMaterialsTemplate(const std::string& templateName,
    DescriptorArenaAllocators& allocators)
{
    return LoadShaderPipelineTemplate({"../assets/shaders/processed/core/material-frag.stage"},
        templateName, allocators);
}

ShaderPipelineTemplate ShaderTemplateLibrary::CreateFromPaths(const std::vector<std::string_view>& paths,
    DescriptorAllocator& allocator)
{
    return ShaderPipelineTemplate({
        .ShaderReflection = ShaderReflection::ReflectFrom(paths),
        .Allocator = &allocator});
}

ShaderPipelineTemplate ShaderTemplateLibrary::CreateFromPaths(const std::vector<std::string_view>& paths,
    DescriptorArenaAllocators& allocators)
{
    return ShaderPipelineTemplate({
        .ShaderReflection = ShaderReflection::ReflectFrom(paths),
        .ResourceAllocator = &allocators.Get(DescriptorAllocatorKind::Resources),
        .SamplerAllocator = &allocators.Get(DescriptorAllocatorKind::Samplers)});
}
