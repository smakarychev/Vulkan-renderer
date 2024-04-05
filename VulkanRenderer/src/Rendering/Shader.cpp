#include "Shader.h"

#include <spirv_reflect.h>

#include <fstream>
#include <ranges>
#include <algorithm>

#include "AssetLib.h"
#include "AssetManager.h"
#include "Buffer.h"
#include "Core/core.h"
#include "Descriptors.h"
#include "Vulkan/Driver.h"
#include "Pipeline.h"
#include "Vulkan/RenderCommand.h"
#include "utils/utils.h"

namespace
{
    // this function is pretty questionable but theoretically we might use some other format for assets
    ShaderStage shaderStageFromAssetStage(u32 stage)
    {
        switch ((SpvReflectShaderStageFlagBits)stage)
        {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
            return ShaderStage::Vertex;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
            return ShaderStage::Pixel;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
            return ShaderStage::Compute;
        default:
            ASSERT(false, "Unsopported shader kind")
        }
        std::unreachable();
    }

    // this function is pretty questionable but theoretically we might use some other format for assets
    ShaderStage shaderStageFromMultipleAssetStages(u32 assetStage)
    {
        u32 typedStage = (SpvReflectShaderStageFlagBits)assetStage;
        ShaderStage stage = ShaderStage::None;
        if ((typedStage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) != 0)
            stage |= ShaderStage::Vertex ;
        if ((typedStage & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT) != 0)
            stage |= ShaderStage::Pixel;
        if ((typedStage & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) != 0)
            stage |= ShaderStage::Compute;

        return stage;
    }

    // this function is pretty questionable but theoretically we might use some other format for assets
    DescriptorType descriptorTypeFromAssetDescriptorType(u32 descriptorType)
    {
        switch ((SpvReflectDescriptorType)descriptorType)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                return DescriptorType::Sampler;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return DescriptorType::Image;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return DescriptorType::ImageStorage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:   return DescriptorType::TexelUniform;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:   return DescriptorType::TexelStorage;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return DescriptorType::UniformBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return DescriptorType::StorageBuffer;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return DescriptorType::UniformBufferDynamic;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return DescriptorType::StorageBufferDynamic;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:       return DescriptorType::Input;
        default:
            ASSERT(false, "Unsupported descriptor type")
            break;
        }
        std::unreachable();
    }

    Format formatFromAssetDescriptorFormat(u32 descriptorFormat)
    {
        switch ((SpvReflectFormat)descriptorFormat)
        {
        case SPV_REFLECT_FORMAT_UNDEFINED:              return Format::Undefined;
        case SPV_REFLECT_FORMAT_R32_UINT:               return Format::R32_UINT;
        case SPV_REFLECT_FORMAT_R32_SINT:               return Format::R32_SINT;
        case SPV_REFLECT_FORMAT_R32_SFLOAT:             return Format::R32_FLOAT;
        case SPV_REFLECT_FORMAT_R32G32_UINT:            return Format::RG32_UINT;
        case SPV_REFLECT_FORMAT_R32G32_SINT:            return Format::RG32_SINT;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:          return Format::RG32_FLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT:         return Format::RGB32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT:         return Format::RGB32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:       return Format::RGB32_FLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:      return Format::RGBA32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:      return Format::RGBA32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:    return Format::RGBA32_FLOAT;
        default:
            ASSERT(false, "Unsupported descriptor format")
            break;
        }
        std::unreachable();
    }

    u32 bindlessDescriptorCount(u32 descriptorType)
    {
        switch ((SpvReflectDescriptorType)descriptorType)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return Driver::GetMaxIndexingImages();
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return Driver::GetMaxIndexingUniformBuffers();
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return Driver::GetMaxIndexingStorageBuffers();
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return Driver::GetMaxIndexingUniformBuffersDynamic();
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return Driver::GetMaxIndexingStorageBuffersDynamic();
        default:
            ASSERT(false, "Unsupported descriptor bindless type")
            break;
        }
        std::unreachable();
    }
}

ShaderModule ShaderModule::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

ShaderModule ShaderModule::Builder::Build(DeletionQueue& deletionQueue)
{
    ShaderModule shader = ShaderModule::Create(m_CreateInfo);
    deletionQueue.Enqueue(shader);

    return shader;
}

ShaderModule ShaderModule::Builder::BuildManualLifetime()
{
    return ShaderModule::Create(m_CreateInfo);
}

ShaderModule::Builder& ShaderModule::Builder::FromSource(const std::vector<u8>& source)
{
    m_CreateInfo.Source = &source;

    return *this;
}

ShaderModule::Builder& ShaderModule::Builder::SetStage(ShaderStage stage)
{
    m_CreateInfo.Stage = stage;

    return *this;
}

ShaderModule ShaderModule::Create(const Builder::CreateInfo& createInfo)
{
    return Driver::Create(createInfo);
}

void ShaderModule::Destroy(const ShaderModule& shader)
{
    Driver::Destroy(shader.Handle());
}

Shader* Shader::ReflectFrom(const std::vector<std::string_view>& paths)
{
    std::string combinedNames;
    for (auto& path : paths)
        combinedNames += std::string{path};

    Shader* cachedShader = AssetManager::GetShader(combinedNames);  
    if (cachedShader)
        return cachedShader;

    Shader shader;

    ShaderStage allStages = ShaderStage::None;
    assetLib::ShaderInfo mergedShaderInfo = {};
    
    for (auto& path : paths)
    {
        assetLib::ShaderInfo shaderAsset = shader.LoadFromAsset(path);
        allStages |= shaderStageFromAssetStage(shaderAsset.ShaderStages);
        mergedShaderInfo = MergeReflections(mergedShaderInfo, shaderAsset);
    }

    std::ranges::sort(mergedShaderInfo.DescriptorSets, [](auto& a, auto& b) { return a.Set < b.Set; });
    for (auto& set : mergedShaderInfo.DescriptorSets)
    {
        std::ranges::sort(set.Descriptors, [](auto& a, auto& b) { return a.Binding < b.Binding; });

        // assert that bindings starts with 0 and have no holes
        for (u32 bindingIndex = 0; bindingIndex < set.Descriptors.size(); bindingIndex++)
            ASSERT(set.Descriptors[bindingIndex].Binding == bindingIndex,
                "Descriptor with index {} must have a binding of {}", bindingIndex, bindingIndex)
    }

    ASSERT(mergedShaderInfo.DescriptorSets.size() <= MAX_PIPELINE_DESCRIPTOR_SETS,
        "Can have only {} different descriptor sets, but have {}",
        MAX_PIPELINE_DESCRIPTOR_SETS, mergedShaderInfo.DescriptorSets.size())

    ASSERT(mergedShaderInfo.PushConstants.size() <= 1, "Only one push constant is supported")

    shader.m_ReflectionData = {
        .ShaderStages = allStages,
        .SpecializationConstants = mergedShaderInfo.SpecializationConstants,
        .InputAttributes = mergedShaderInfo.InputAttributes,
        .PushConstants = mergedShaderInfo.PushConstants,
        .DescriptorSets = ProcessDescriptorSets(mergedShaderInfo.DescriptorSets),
    };

    AssetManager::AddShader(combinedNames, shader);
    
    return AssetManager::GetShader(combinedNames);
}

assetLib::ShaderInfo Shader::LoadFromAsset(std::string_view path)
{
    assetLib::File shaderFile;
    assetLib::loadAssetFile(path, shaderFile);
    assetLib::ShaderInfo shaderInfo = assetLib::readShaderInfo(shaderFile);
    
    ShaderModuleSource shaderModule = {};
    shaderModule.Stage = shaderStageFromAssetStage(shaderInfo.ShaderStages);
    shaderModule.Source.resize(shaderInfo.SourceSizeBytes);
    assetLib::unpackShader(shaderInfo, shaderFile.Blob.data(), shaderFile.Blob.size(), shaderModule.Source.data());
    m_Modules.push_back(shaderModule);

    return shaderInfo;
}

assetLib::ShaderInfo Shader::MergeReflections(const assetLib::ShaderInfo& first, const assetLib::ShaderInfo& second)
{
    ASSERT(!(first.ShaderStages & second.ShaderStages), "Overlapping shader stages")
    ASSERT(((first.ShaderStages | second.ShaderStages) & VK_SHADER_STAGE_COMPUTE_BIT) == 0 ||
           ((first.ShaderStages | second.ShaderStages) & VK_SHADER_STAGE_COMPUTE_BIT) == VK_SHADER_STAGE_COMPUTE_BIT,
           "Compute shaders cannot be combined with others in pipeline")

    assetLib::ShaderInfo merged = first;

    merged.ShaderStages |= second.ShaderStages;

    // merge specialization constants
    merged.SpecializationConstants = utils::mergeSets(merged.SpecializationConstants, second.SpecializationConstants,
        [](const auto& a, const auto& b)
        {
            if (a.Id == b.Id)
                return 0;
            if (a.Id < b.Id)
                return -1;
            return 1;
        },
        [](const auto& a, const auto& b)
        {
            ASSERT(a.Name == b.Name, "Specialization constants have same id but different name")
            ReflectionData::SpecializationConstant merged = a;
            merged.ShaderStages |= b.ShaderStages;
            return merged;
        });
    
    // merge inputs (possibly nothing happens)
    merged.InputAttributes.append_range(second.InputAttributes);

    // merge push constants
    merged.PushConstants = utils::mergeSets(merged.PushConstants, second.PushConstants,
        [](const auto& a, const auto& b)
        {
            if (a.Offset == b.Offset && a.SizeBytes == b.SizeBytes)
                return 0;   
            if (a.Offset < b.Offset || a.Offset == b.Offset && a.SizeBytes < b.SizeBytes)
                return -1;
            return 1;
        },
        [](const auto& a, const auto& b)
        {
            assetLib::ShaderInfo::PushConstant merged = a;
            merged.ShaderStages |= b.ShaderStages;
            return merged;
        });

    // merge descriptor sets
    merged.DescriptorSets = utils::mergeSets(merged.DescriptorSets, second.DescriptorSets,
        [](const auto& a, const auto& b)
        {
            if (a.Set == b.Set)
               return 0;
            if (a.Set < b.Set)
               return -1;
            return 1;
        },
        [](const auto& a, const auto& b)
        {
            assetLib::ShaderInfo::DescriptorSet mergedSet = a;
            mergedSet.Descriptors = utils::mergeSets(mergedSet.Descriptors, b.Descriptors,
                [](const auto& a, const auto& b)
                {
                    if (a.Binding == b.Binding)
                        return 0;   
                    if (a.Binding < b.Binding)
                        return -1;
                    return 1;
                },
                [](const auto& a, const auto& b)
                {
                    ASSERT(a.Name == b.Name, "Descriptors have same binding but different names")
                    assetLib::ShaderInfo::DescriptorSet::DescriptorBinding mergedDescriptor = a;
                    mergedDescriptor.ShaderStages |= b.ShaderStages;

                    return mergedDescriptor;
               });

            return mergedSet;
        });

    return merged;
}

std::vector<Shader::ReflectionData::DescriptorSet> Shader::ProcessDescriptorSets(
    const std::vector<assetLib::ShaderInfo::DescriptorSet>& sets)
{
    std::vector<ReflectionData::DescriptorSet> descriptorSets(sets.size());
    for (u32 setIndex = 0; setIndex < descriptorSets.size(); setIndex++)
    {
        descriptorSets[setIndex].Set = sets[setIndex].Set;
        descriptorSets[setIndex].Descriptors.resize(sets[setIndex].Descriptors.size());
        bool containsBindlessDescriptors = false;
        bool containsImmutableSamplers = false;
        for (u32 descriptorIndex = 0; descriptorIndex < sets[setIndex].Descriptors.size(); descriptorIndex++)
        {
            auto& descriptor = descriptorSets[setIndex].Descriptors[descriptorIndex];
            descriptor.Descriptor.Binding = sets[setIndex].Descriptors[descriptorIndex].Binding;
            descriptor.Name = sets[setIndex].Descriptors[descriptorIndex].Name;
            descriptor.Descriptor.Type = descriptorTypeFromAssetDescriptorType(
                sets[setIndex].Descriptors[descriptorIndex].Type);
            descriptor.Descriptor.Shaders = shaderStageFromMultipleAssetStages(
                sets[setIndex].Descriptors[descriptorIndex].ShaderStages);
            descriptor.Descriptor.DescriptorFlags = sets[setIndex].Descriptors[descriptorIndex].Flags;

            if (enumHasAny(sets[setIndex].Descriptors[descriptorIndex].Flags,
                assetLib::ShaderInfo::DescriptorSet::Bindless))
            {
                containsBindlessDescriptors = true;
                descriptor.Descriptor.Count = bindlessDescriptorCount(
                    sets[setIndex].Descriptors[descriptorIndex].Type);
            }
            else
            {
                descriptor.Descriptor.Count = 1;
            }
            // every specific version of ImmutableSampler flag also contains the base flag
            containsImmutableSamplers = containsImmutableSamplers ||
                enumHasAny(descriptor.Descriptor.DescriptorFlags,
                    assetLib::ShaderInfo::DescriptorSet::ImmutableSampler);
        }
        descriptorSets[setIndex].HasBindless = containsBindlessDescriptors;
        descriptorSets[setIndex].HasImmutableSampler = containsImmutableSamplers;
    }

    return descriptorSets;
}

ShaderPipelineTemplate ShaderPipelineTemplate::Builder::Build()
{
    return Build(Driver::DeletionQueue());
}

ShaderPipelineTemplate ShaderPipelineTemplate::Builder::Build(DeletionQueue& deletionQueue)
{
    ShaderPipelineTemplate shaderPipelineTemplate = ShaderPipelineTemplate::Create(m_CreateInfo);
    for (auto& shader : shaderPipelineTemplate.m_Shaders)
        deletionQueue.Enqueue(shader);

    return shaderPipelineTemplate;
}

ShaderPipelineTemplate ShaderPipelineTemplate::Builder::BuildManualLifetime()
{
    return ShaderPipelineTemplate::Create(m_CreateInfo);
}

ShaderPipelineTemplate::Builder& ShaderPipelineTemplate::Builder::SetShaderReflection(Shader* shaderReflection)
{
    m_CreateInfo.ShaderReflection = shaderReflection;

    return *this;
}

ShaderPipelineTemplate::Builder& ShaderPipelineTemplate::Builder::SetDescriptorAllocator(DescriptorAllocator* allocator)
{
    ASSERT(m_CreateInfo.ResourceAllocator == nullptr && m_CreateInfo.SamplerAllocator == nullptr,
        "Cannot set both allocator and arena allocator")
    m_CreateInfo.Allocator = allocator;

    return *this;
}

ShaderPipelineTemplate::Builder& ShaderPipelineTemplate::Builder::SetDescriptorArenaResourceAllocator(
    DescriptorArenaAllocator* allocator)
{
    ASSERT(m_CreateInfo.Allocator == nullptr, "Cannot set both allocator and arena allocator")
    m_CreateInfo.ResourceAllocator = allocator;

    return *this;
}

ShaderPipelineTemplate::Builder& ShaderPipelineTemplate::Builder::SetDescriptorArenaSamplerAllocator(
    DescriptorArenaAllocator* allocator)
{
    ASSERT(m_CreateInfo.Allocator == nullptr, "Cannot set both allocator and arena allocator")
    m_CreateInfo.SamplerAllocator = allocator;

    return *this;
}

ShaderPipelineTemplate ShaderPipelineTemplate::Create(const Builder::CreateInfo& createInfo)
{
    ShaderPipelineTemplate shaderPipelineTemplate = {};

    if (createInfo.Allocator == nullptr)
    {
        shaderPipelineTemplate.m_UseDescriptorBuffer = true;
        shaderPipelineTemplate.m_Allocator.ResourceAllocator = createInfo.ResourceAllocator;
        shaderPipelineTemplate.m_Allocator.SamplerAllocator = createInfo.SamplerAllocator;
    }
    else
    {
        shaderPipelineTemplate.m_Allocator.DescriptorAllocator = createInfo.Allocator;
    }
    
    const auto& reflectionData = createInfo.ShaderReflection->GetReflectionData();
    
    shaderPipelineTemplate.m_DescriptorsLayouts = CreateDescriptorLayouts(reflectionData.DescriptorSets,
        shaderPipelineTemplate.m_UseDescriptorBuffer);
    shaderPipelineTemplate.m_VertexInputDescription = CreateInputDescription(reflectionData.InputAttributes);
    std::vector<ShaderPushConstantDescription> pushConstantDescriptions = CreatePushConstantDescriptions(reflectionData.PushConstants);
    std::vector<ShaderModule> shaderModules = CreateShaderModules(createInfo.ShaderReflection->GetShadersSource());
    
    shaderPipelineTemplate.m_PipelineLayout = PipelineLayout::Builder()
        .SetPushConstants(pushConstantDescriptions)
        .SetDescriptorLayouts(shaderPipelineTemplate.m_DescriptorsLayouts)
        .Build();
    
    shaderPipelineTemplate.m_PipelineBuilder = Pipeline::Builder()
        .SetLayout(shaderPipelineTemplate.m_PipelineLayout);

    shaderPipelineTemplate.m_Shaders.reserve(shaderModules.size());
    for (auto& shader : shaderModules)
    {
        shaderPipelineTemplate.m_PipelineBuilder.AddShader(shader);
        shaderPipelineTemplate.m_Shaders.push_back(shader);
    }

    if (shaderModules.size() == 1 && shaderModules.front().m_Stage == ShaderStage::Compute)
        shaderPipelineTemplate.m_PipelineBuilder.IsComputePipeline(true);

    shaderPipelineTemplate.m_SpecializationConstants.reserve(reflectionData.SpecializationConstants.size());
    for (auto& constant : reflectionData.SpecializationConstants)
        shaderPipelineTemplate.m_SpecializationConstants.push_back(constant);
    
    for (auto& set : reflectionData.DescriptorSets)
    {
        auto& setInfo = shaderPipelineTemplate.m_DescriptorSetsInfo[set.Set];
        setInfo.Names.reserve(set.Descriptors.size());
        setInfo.Bindings.reserve(set.Descriptors.size());
        for (auto& descriptor : set.Descriptors)
        {
            setInfo.Names.push_back(descriptor.Name);
            setInfo.Bindings.push_back(descriptor.Descriptor);
        }
        
        shaderPipelineTemplate.m_DescriptorPoolFlags.push_back(set.HasBindless ?
            DescriptorPoolFlags::UpdateAfterBind : DescriptorPoolFlags::None);
    }
        
    shaderPipelineTemplate.m_DescriptorSetCount = (u32)reflectionData.DescriptorSets.size();
    
    return shaderPipelineTemplate;
}

void ShaderPipelineTemplate::Destroy(const ShaderPipelineTemplate& shaderPipelineTemplate)
{
    for (auto& shader : shaderPipelineTemplate.m_Shaders)
        ShaderModule::Destroy(shader);
}

const DescriptorBinding& ShaderPipelineTemplate::GetBinding(u32 set, std::string_view name) const
{
    const DescriptorBinding* binding = TryGetBinding(set, name);

    ASSERT(binding != nullptr, "Unrecognized descriptor binding name")
    
    return *binding;
}

const DescriptorBinding* ShaderPipelineTemplate::TryGetBinding(u32 set, std::string_view name) const
{
    auto& setInfo = m_DescriptorSetsInfo[set];
    for (u32 descriptorIndex = 0; descriptorIndex < setInfo.Bindings.size(); descriptorIndex++)
    {
        if (setInfo.Names[descriptorIndex] == name)
            return &setInfo.Bindings[descriptorIndex];
    }

    return nullptr;
}

std::pair<u32, const DescriptorBinding&> ShaderPipelineTemplate::GetSetAndBinding(std::string_view name) const
{
    const DescriptorBinding* descriptorBinding = nullptr;
    u32 set = 0;
    for (u32 setIndex = 0; setIndex < m_DescriptorSetsInfo.size(); setIndex++)
    {
        set = setIndex;
        descriptorBinding = TryGetBinding(setIndex, name);
        if (descriptorBinding != nullptr)
            break;
    }
    ASSERT(descriptorBinding != nullptr, "No such binding exists")

    return {set, *descriptorBinding};
}

std::array<bool, MAX_PIPELINE_DESCRIPTOR_SETS> ShaderPipelineTemplate::GetSetPresence() const
{
    std::array<bool, MAX_PIPELINE_DESCRIPTOR_SETS> presence = {};
    for (u32 setIndex = 0; setIndex < m_DescriptorSetsInfo.size(); setIndex++)
        presence[setIndex] = !m_DescriptorSetsInfo[setIndex].Bindings.empty();

    return presence;
}

bool ShaderPipelineTemplate::IsComputeTemplate() const
{
    return m_Shaders.size() == 1 && m_Shaders.front().m_Stage == ShaderStage::Compute;
}

std::vector<DescriptorsLayout> ShaderPipelineTemplate::CreateDescriptorLayouts(
    const std::vector<ReflectionData::DescriptorSet>& descriptorSetReflections, bool useDescriptorBuffer)
{
    if (descriptorSetReflections.empty())
        return {};
    
    std::vector<DescriptorsLayout> layouts;
    layouts.reserve(descriptorSetReflections.size());

    static const DescriptorsLayout EMPTY_LAYOUT_ORDINARY = DescriptorsLayout::Builder().Build(); 
    static const DescriptorsLayout EMPTY_LAYOUT_DESCRIPTOR_BUFFER = DescriptorsLayout::Builder()
        .SetFlags(DescriptorLayoutFlags::DescriptorBuffer)
        .Build();

    const DescriptorsLayout* EMPTY_LAYOUT = useDescriptorBuffer ?
        &EMPTY_LAYOUT_DESCRIPTOR_BUFFER : &EMPTY_LAYOUT_ORDINARY;
    for (u32 emptySetIndex = 0; emptySetIndex < descriptorSetReflections.front().Set; emptySetIndex++)
        layouts.push_back(*EMPTY_LAYOUT);
    
    for (auto& set : descriptorSetReflections)
    {
        DescriptorsFlags descriptorsFlags = ExtractDescriptorsAndFlags(set, useDescriptorBuffer);
    
        DescriptorLayoutFlags layoutFlags = set.HasImmutableSampler ?
            DescriptorLayoutFlags::EmbeddedImmutableSamplers : DescriptorLayoutFlags::None;
        if (useDescriptorBuffer)
            layoutFlags |= DescriptorLayoutFlags::DescriptorBuffer;
        else if (set.HasBindless)
            layoutFlags |= DescriptorLayoutFlags::UpdateAfterBind;
        
        DescriptorsLayout layout = DescriptorsLayout::Builder()
            .SetBindings(descriptorsFlags.Descriptors)
            .SetBindingFlags(descriptorsFlags.Flags)
            .SetFlags(layoutFlags)
            .Build();
        layouts.push_back(layout);
    }

    return layouts;
}

VertexInputDescription ShaderPipelineTemplate::CreateInputDescription(
    const std::vector<ReflectionData::InputAttribute>& inputAttributeReflections)
{
    VertexInputDescription inputDescription = {};

    static constexpr u32 SPARSE_NONE = std::numeric_limits<u32>::max();
    std::vector<u32> bindingsDense;
    std::vector<u32> bindingsSparse;

    for (auto& input : inputAttributeReflections)
    {
        if (input.Binding >= bindingsSparse.size())
            bindingsSparse.resize(input.Binding + 1, SPARSE_NONE);

        if (bindingsSparse[input.Binding] == SPARSE_NONE)
        {
            bindingsSparse[input.Binding] = (u32)bindingsDense.size();
            bindingsDense.push_back(input.Binding);
        }
    }
    
    inputDescription.Bindings.reserve(bindingsDense.size());
    inputDescription.Attributes.reserve(inputAttributeReflections.size());

    for (u32 bindingIndex : bindingsDense)
    {
        VertexInputDescription::Binding binding = {
            .Index = bindingIndex,
            .StrideBytes = 0};
        inputDescription.Bindings.push_back(binding);
    }
    
    for (auto& input : inputAttributeReflections)
    {
        u32 bindingIndex = bindingsDense[bindingsSparse[input.Binding]];
        auto& binding = inputDescription.Bindings[bindingsSparse[input.Binding]];
        VertexInputDescription::Attribute attribute = {
            .Index = input.Location,
            .BindingIndex = bindingIndex,
            .Format = formatFromAssetDescriptorFormat(input.Format),
            .OffsetBytes = binding.StrideBytes};
        binding.StrideBytes += input.SizeBytes;

        inputDescription.Attributes.push_back(attribute);
    }

    return inputDescription;
}

std::vector<ShaderPushConstantDescription> ShaderPipelineTemplate::CreatePushConstantDescriptions(
    const std::vector<ReflectionData::PushConstant>& pushConstantReflections)
{
    std::vector<ShaderPushConstantDescription> pushConstants;
    pushConstants.reserve(pushConstantReflections.size());

    for (auto& pushConstant : pushConstantReflections)
    {
        ShaderPushConstantDescription description = {};
        description.SizeBytes = pushConstant.SizeBytes;
        description.Offset = pushConstant.Offset;
        description.StageFlags = shaderStageFromMultipleAssetStages(pushConstant.ShaderStages);

        pushConstants.push_back(description);
    }

    return pushConstants;
}

std::vector<ShaderModule> ShaderPipelineTemplate::CreateShaderModules(
    const std::vector<Shader::ShaderModuleSource>& shaders)
{
    std::vector<ShaderModule> shaderModules;
    shaderModules.reserve(shaders.size());

    for (auto& shader : shaders)
    {
        ShaderModule module = ShaderModule::Builder()
            .FromSource(shader.Source)
            .SetStage(shader.Stage)
            .BuildManualLifetime();

        shaderModules.push_back(module);
    }

    return shaderModules;
}

ShaderPipelineTemplate::DescriptorsFlags ShaderPipelineTemplate::ExtractDescriptorsAndFlags(
    const ReflectionData::DescriptorSet& descriptorSet, bool useDescriptorBuffer)
{
    DescriptorsFlags descriptorsFlags;
    descriptorsFlags.Descriptors.reserve(descriptorSet.Descriptors.size());
    descriptorsFlags.Flags.reserve(descriptorSet.Descriptors.size());
    
    for (auto& descriptor : descriptorSet.Descriptors)
    {
        descriptorsFlags.Descriptors.push_back(descriptor.Descriptor);
        DescriptorFlags flags = DescriptorFlags::None;
        if (enumHasAny(descriptor.Descriptor.DescriptorFlags, assetLib::ShaderInfo::DescriptorSet::Bindless))
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

ShaderPipeline ShaderPipeline::Builder::Build()
{
    Prebuild();
    
    return ShaderPipeline::Create(m_CreateInfo);
}

ShaderPipeline::Builder& ShaderPipeline::Builder::SetRenderingDetails(const RenderingDetails& renderingDetails)
{
    m_CreateInfo.RenderingDetails = renderingDetails;
    
    return *this;
}

ShaderPipeline::Builder& ShaderPipeline::Builder::DepthMode(::DepthMode depthMode)
{
    m_DepthMode = depthMode;

    return *this;
}

ShaderPipeline::Builder& ShaderPipeline::Builder::PrimitiveKind(::PrimitiveKind primitiveKind)
{
    m_PrimitiveKind = primitiveKind;

    return *this;
}

ShaderPipeline::Builder& ShaderPipeline::Builder::AlphaBlending(::AlphaBlending alphaBlending)
{
    m_AlphaBlending = alphaBlending;

    return *this;
}

ShaderPipeline::Builder& ShaderPipeline::Builder::SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate)
{
    m_CreateInfo.ShaderPipelineTemplate = shaderPipelineTemplate;

    return *this;
}

ShaderPipeline::Builder& ShaderPipeline::Builder::CompatibleWithVertex(
    const VertexInputDescription& vertexInputDescription)
{
    m_CompatibleVertexDescription = vertexInputDescription;

    return *this;
}

ShaderPipeline::Builder& ShaderPipeline::Builder::UseDescriptorBuffer()
{
    m_CreateInfo.UseDescriptorBuffer = true;

    return *this;
}

void ShaderPipeline::Builder::Prebuild()
{
    m_CreateInfo.ShaderPipelineTemplate->m_PipelineBuilder.DepthMode(m_DepthMode);
    m_CreateInfo.ShaderPipelineTemplate->m_PipelineBuilder.PrimitiveKind(m_PrimitiveKind);
    m_CreateInfo.ShaderPipelineTemplate->m_PipelineBuilder.AlphaBlending(m_AlphaBlending);
    
    if (!m_CompatibleVertexDescription.Bindings.empty())
        CreateCompatibleLayout();

    if (m_CreateInfo.ShaderPipelineTemplate->IsComputeTemplate())
        ASSERT(m_CreateInfo.RenderingDetails.ColorFormats.empty(),
            "Compute shader pipeline does not need rendering details")

    FinishSpecializationConstants();
}

void ShaderPipeline::Builder::CreateCompatibleLayout()
{
    // adapt vertex input layout
    const VertexInputDescription& available = m_CreateInfo.ShaderPipelineTemplate->m_VertexInputDescription;
    const VertexInputDescription& compatible = m_CompatibleVertexDescription;
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

    m_CreateInfo.ShaderPipelineTemplate->m_PipelineBuilder.SetVertexDescription(adapted);
}

void ShaderPipeline::Builder::FinishSpecializationConstants()
{
    for (u32 specializationIndex = 0; specializationIndex < m_SpecializationConstantNames.size(); specializationIndex++)
    {
        std::string_view name = m_SpecializationConstantNames[specializationIndex];
        auto it = std::ranges::find(m_CreateInfo.ShaderPipelineTemplate->m_SpecializationConstants, name,
            [](auto& constant) { return constant.Name; });
        ASSERT(it != m_CreateInfo.ShaderPipelineTemplate->m_SpecializationConstants.end(),
            "Unrecognized specialization name")
        m_PipelineSpecializationInfo.ShaderSpecializations[specializationIndex].Id = it->Id;
        m_PipelineSpecializationInfo.ShaderSpecializations[specializationIndex].ShaderStages =
            shaderStageFromMultipleAssetStages(it->ShaderStages);
    }

    m_CreateInfo.PipelineSpecializationInfo = m_PipelineSpecializationInfo;
}

ShaderPipeline ShaderPipeline::Create(const Builder::CreateInfo& createInfo)
{
    ShaderPipeline shaderPipeline = {};
    
    shaderPipeline.m_Template = createInfo.ShaderPipelineTemplate;

    // use local builder to not worry about state
    Pipeline::Builder pipelineBuilder = shaderPipeline.m_Template->m_PipelineBuilder;
    if (!createInfo.ShaderPipelineTemplate->IsComputeTemplate())
        pipelineBuilder.SetRenderingDetails(createInfo.RenderingDetails);

    pipelineBuilder.UseSpecialization(createInfo.PipelineSpecializationInfo);
    if (createInfo.UseDescriptorBuffer)
        pipelineBuilder.UseDescriptorBuffer();
    
    if (createInfo.ShaderPipelineTemplate->IsComputeTemplate())
        shaderPipeline.m_Pipeline = pipelineBuilder.Build();
    else
        shaderPipeline.m_Pipeline = pipelineBuilder.SetRenderingDetails(createInfo.RenderingDetails).Build();    

    return shaderPipeline;
}

void ShaderPipeline::BindGraphics(const CommandBuffer& cmd) const
{
    m_Pipeline.BindGraphics(cmd);
}

void ShaderPipeline::BindCompute(const CommandBuffer& cmd) const
{
    m_Pipeline.BindCompute(cmd);
}

ShaderDescriptorSet ShaderDescriptorSet::Builder::Build()
{
    PreBuild();
    ShaderDescriptorSet descriptorSet = ShaderDescriptorSet::Create(m_CreateInfo);
    m_CreateInfo.DescriptorBuilders = {};

    return descriptorSet;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::SetTemplate(ShaderPipelineTemplate* shaderPipelineTemplate)
{
    m_CreateInfo.ShaderPipelineTemplate = shaderPipelineTemplate;

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name, const Buffer& buffer)
{
    AddBinding(name, buffer, buffer.GetSizeBytes(), 0);

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name,
    const Buffer& buffer, u64 sizeBytes, u64 offset)
{
    auto&& [set, descriptorBinding] = m_CreateInfo.ShaderPipelineTemplate->GetSetAndBinding(name);

    m_CreateInfo.DescriptorBuilders[set].AddBufferBinding(
        descriptorBinding.Binding, buffer.Subresource(sizeBytes, offset), descriptorBinding.Type);

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name,
    const DescriptorSet::TextureBindingInfo& texture)
{
    auto&& [set, descriptorBinding] = m_CreateInfo.ShaderPipelineTemplate->GetSetAndBinding(name);

    m_CreateInfo.DescriptorBuilders[set].AddTextureBinding(
        descriptorBinding.Binding,
        texture,
        descriptorBinding.Type);

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name, u32 variableBindingCount)
{
    auto&& [set, descriptorBinding] = m_CreateInfo.ShaderPipelineTemplate->GetSetAndBinding(name);

    m_CreateInfo.DescriptorBuilders[set].AddVariableBinding({
        .Slot = descriptorBinding.Binding,
        .Count = variableBindingCount});

    return *this;
}

void ShaderDescriptorSet::Builder::PreBuild()
{
    ASSERT(!m_CreateInfo.ShaderPipelineTemplate->m_UseDescriptorBuffer,
        "ShaderPipelineTemplate was configured to use descriptor buffer, and therefore cannot be used to create"
        " shader descriptor set")
    m_CreateInfo.SetPresence = m_CreateInfo.ShaderPipelineTemplate->GetSetPresence();
    
    u32 descriptorCount = m_CreateInfo.ShaderPipelineTemplate->m_DescriptorSetCount;
    for (u32 i = 0; i < descriptorCount; i++)
    {
        m_CreateInfo.DescriptorBuilders[i].SetAllocator(
            m_CreateInfo.ShaderPipelineTemplate->m_Allocator.DescriptorAllocator);
        if (m_CreateInfo.SetPresence[i])
        {
            m_CreateInfo.DescriptorBuilders[i].SetLayout(
                m_CreateInfo.ShaderPipelineTemplate->GetDescriptorsLayout(i));
            m_CreateInfo.DescriptorBuilders[i].SetPoolFlags(
                m_CreateInfo.ShaderPipelineTemplate->m_DescriptorPoolFlags[i]);
        }
    }
}

ShaderDescriptorSet ShaderDescriptorSet::Create(const Builder::CreateInfo& createInfo)
{
    ShaderDescriptorSet descriptorSet = {};

    descriptorSet.m_Template = createInfo.ShaderPipelineTemplate;

    u32 setCount = 0;
    for (u32 i = 0; i < MAX_PIPELINE_DESCRIPTOR_SETS; i++)
    {
        if (createInfo.SetPresence[i] == 0)
            continue;
        
        descriptorSet.m_DescriptorSetsInfo.DescriptorSets[i].IsPresent = true;
        descriptorSet.m_DescriptorSetsInfo.DescriptorSets[i].Set = const_cast<DescriptorSet::Builder&>(
            createInfo.DescriptorBuilders[i]).Build();
        setCount++;
    }
    descriptorSet.m_DescriptorSetsInfo.DescriptorCount = setCount;

    return descriptorSet;
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

    m_DescriptorSetsInfo.DescriptorSets[set].Set.SetTexture(
        descriptorBinding.Binding,
        texture,
        descriptorBinding.Type,
        arrayIndex);
}

ShaderDescriptors ShaderDescriptors::Builder::Build()
{
    return ShaderDescriptors::Create(m_CreateInfo);
}

ShaderDescriptors::Builder& ShaderDescriptors::Builder::SetTemplate(
    const ShaderPipelineTemplate* shaderPipelineTemplate, DescriptorAllocatorKind allocatorKind)
{
    ASSERT(shaderPipelineTemplate->m_UseDescriptorBuffer,
        "Shader pipeline template is not configured to be used with descriptor buffer")
    m_CreateInfo.ShaderPipelineTemplate = shaderPipelineTemplate;
    m_CreateInfo.Allocator = allocatorKind == DescriptorAllocatorKind::Resources ?
        shaderPipelineTemplate->m_Allocator.ResourceAllocator : 
        shaderPipelineTemplate->m_Allocator.SamplerAllocator;
    ASSERT(m_CreateInfo.Allocator, "Allocator is unset")

    return *this;
}

ShaderDescriptors::Builder& ShaderDescriptors::Builder::ExtractSet(u32 set)
{
    m_CreateInfo.Set = set;

    return *this;
}

ShaderDescriptors::Builder& ShaderDescriptors::Builder::BindlessCount(u32 count)
{
    m_CreateInfo.BindlessCount = count;

    return *this;
}

ShaderDescriptors ShaderDescriptors::Create(const Builder::CreateInfo& createInfo)
{
    auto* shaderTemplate = createInfo.ShaderPipelineTemplate;

    Descriptors descriptors = createInfo.Allocator->Allocate(
        shaderTemplate->GetDescriptorsLayout(createInfo.Set), {
            .Bindings = shaderTemplate->m_DescriptorSetsInfo[createInfo.Set].Bindings,
            .BindlessCount = createInfo.BindlessCount});

    ShaderDescriptors shaderDescriptors = {};
    shaderDescriptors.m_Descriptors = descriptors;
    shaderDescriptors.m_SetNumber = createInfo.Set;
    shaderDescriptors.m_Template = createInfo.ShaderPipelineTemplate;
    
    return shaderDescriptors;
}

void ShaderDescriptors::BindGraphics(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout) const
{
    m_Descriptors.BindGraphics(cmd, allocators, pipelineLayout, m_SetNumber);
}

void ShaderDescriptors::BindCompute(const CommandBuffer& cmd, const DescriptorArenaAllocators& allocators,
    PipelineLayout pipelineLayout) const
{
    m_Descriptors.BindCompute(cmd, allocators, pipelineLayout, m_SetNumber);
}

void ShaderDescriptors::BindGraphicsImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const
{
    m_Descriptors.BindGraphicsImmutableSamplers(cmd, pipelineLayout, m_SetNumber);
}

void ShaderDescriptors::BindComputeImmutableSamplers(const CommandBuffer& cmd, PipelineLayout pipelineLayout) const
{
    m_Descriptors.BindComputeImmutableSamplers(cmd, pipelineLayout, m_SetNumber);
}

void ShaderDescriptors::UpdateBinding(std::string_view name, const BufferBindingInfo& buffer) const
{
    m_Descriptors.UpdateBinding(GetBindingInfo(name), buffer);
}

void ShaderDescriptors::UpdateBinding(std::string_view name, const TextureBindingInfo& texture) const
{
    m_Descriptors.UpdateBinding(GetBindingInfo(name), texture);
}

void ShaderDescriptors::UpdateBinding(std::string_view name, const TextureBindingInfo& texture, u32 bindlessIndex) const
{
    m_Descriptors.UpdateBinding(GetBindingInfo(name), texture, bindlessIndex);
}

void ShaderDescriptors::UpdateBinding(const BindingInfo& bindingInfo, const BufferBindingInfo& buffer) const
{
    m_Descriptors.UpdateBinding(bindingInfo, buffer);
}

void ShaderDescriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture) const
{
    m_Descriptors.UpdateBinding(bindingInfo, texture);
}

void ShaderDescriptors::UpdateBinding(const BindingInfo& bindingInfo, const TextureBindingInfo& texture,
    u32 bindlessIndex) const
{
    m_Descriptors.UpdateBinding(bindingInfo, texture, bindlessIndex);
}

ShaderDescriptors::BindingInfo ShaderDescriptors::GetBindingInfo(std::string_view bindingName) const
{
    auto& binding = m_Template->GetBinding(m_SetNumber, bindingName);

    return {.Slot = binding.Binding, .Type = binding.Type};
}

std::unordered_map<std::string, ShaderPipelineTemplate> ShaderTemplateLibrary::m_Templates = {};

ShaderPipelineTemplate* ShaderTemplateLibrary::LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
    std::string_view templateName, DescriptorAllocator& allocator)
{
    std::string name = GenerateTemplateName(templateName, allocator);
    if (!GetShaderTemplate(name))
    {
        Shader* shaderReflection = Shader::ReflectFrom(paths);

        ShaderPipelineTemplate shaderTemplate = ShaderPipelineTemplate::Builder()
            .SetDescriptorAllocator(&allocator)
            .SetShaderReflection(shaderReflection)
            .Build();
        
        AddShaderTemplate(shaderTemplate, name);
    }
    
    return GetShaderTemplate(name);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
    std::string_view templateName, DescriptorArenaAllocators& allocators)
{
    std::string name = GenerateTemplateName(templateName, allocators);
    
    if (!GetShaderTemplate(name))
    {
        Shader* shaderReflection = Shader::ReflectFrom(paths);

        ShaderPipelineTemplate shaderTemplate = ShaderPipelineTemplate::Builder()
            .SetDescriptorArenaResourceAllocator(&allocators.Get(DescriptorAllocatorKind::Resources))
            .SetDescriptorArenaSamplerAllocator(&allocators.Get(DescriptorAllocatorKind::Samplers))
            .SetShaderReflection(shaderReflection)
            .Build();
        
        AddShaderTemplate(shaderTemplate, name);
    }
    
    return GetShaderTemplate(name);
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(const std::string& name,
    DescriptorArenaAllocators& allocators)
{
    return GetShaderTemplate(GenerateTemplateName(name, allocators));
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(const std::string& name)
{
    auto it = m_Templates.find(name);
    return it == m_Templates.end() ? nullptr : &it->second;
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
    m_Templates.emplace(std::make_pair(name, shaderTemplate));
}
