#include "Shader.h"

#include <spirv_reflect.h>

#include <fstream>

#include "AssetLib.h"
#include "AssetManager.h"
#include "Buffer.h"
#include "Core/core.h"
#include "DescriptorSet.h"
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
        if ((assetStage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) != 0)
            stage |= ShaderStage::Vertex ;
        if ((assetStage & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT) != 0)
            stage |= ShaderStage::Pixel;
        if ((assetStage & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) != 0)
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
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return DescriptorType::ImageSampler;
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
    deletionQueue.AddDeleter([shader]() { ShaderModule::Destroy(shader); });

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
    Driver::Destroy(shader);
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
        for (u32 descriptorIndex = 0; descriptorIndex < sets[setIndex].Descriptors.size(); descriptorIndex++)
        {
            auto& descriptor = descriptorSets[setIndex].Descriptors[descriptorIndex];
            descriptor.Descriptor.Binding = sets[setIndex].Descriptors[descriptorIndex].Binding;
            descriptor.Name = sets[setIndex].Descriptors[descriptorIndex].Name;
            descriptor.Descriptor.Type = descriptorTypeFromAssetDescriptorType(
                sets[setIndex].Descriptors[descriptorIndex].Type);
            descriptor.Descriptor.Shaders = shaderStageFromMultipleAssetStages(
                sets[setIndex].Descriptors[descriptorIndex].ShaderStages);

            if (sets[setIndex].Descriptors[descriptorIndex].Flags & assetLib::ShaderInfo::DescriptorSet::Bindless)
            {
                containsBindlessDescriptors = true;
                descriptor.Descriptor.Count = bindlessDescriptorCount(
                    sets[setIndex].Descriptors[descriptorIndex].Type);
                descriptor.Flags =
                    DescriptorFlags::VariableCount |
                    DescriptorFlags::PartiallyBound |
                    DescriptorFlags::UpdateAfterBind |
                    DescriptorFlags::UpdateUnusedPending;
            }
            else
            {
                descriptor.Descriptor.Count = 1;
            }
            
            descriptor.Descriptor.HasImmutableSampler = sets[setIndex].Descriptors[descriptorIndex].Flags &
                assetLib::ShaderInfo::DescriptorSet::ImmutableSampler;
        }
        if (containsBindlessDescriptors)
        {
            descriptorSets[setIndex].LayoutFlags = DescriptorSetFlags::UpdateAfterBind;
            descriptorSets[setIndex].PoolFlags = DescriptorPoolFlags::UpdateAfterBind;
        }
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
    deletionQueue.AddDeleter([shaderPipelineTemplate]() { ShaderPipelineTemplate::Destroy(shaderPipelineTemplate); });

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
    m_CreateInfo.Allocator = allocator;

    return *this;
}

ShaderPipelineTemplate ShaderPipelineTemplate::Create(const Builder::CreateInfo& createInfo)
{
    ShaderPipelineTemplate shaderPipelineTemplate = {};

    shaderPipelineTemplate.m_Allocator = createInfo.Allocator;
    
    const auto& reflectionData = createInfo.ShaderReflection->GetReflectionData();
    
    shaderPipelineTemplate.m_DescriptorSetLayouts = CreateDescriptorLayouts(reflectionData.DescriptorSets);
    shaderPipelineTemplate.m_VertexInputDescription = CreateInputDescription(reflectionData.InputAttributes);
    std::vector<ShaderPushConstantDescription> pushConstantDescriptions = CreatePushConstantDescriptions(reflectionData.PushConstants);
    std::vector<ShaderModule> shaderModules = CreateShaderModules(createInfo.ShaderReflection->GetShadersSource());
    
    shaderPipelineTemplate.m_PipelineLayout = PipelineLayout::Builder()
       .SetPushConstants(pushConstantDescriptions)
       .SetDescriptorLayouts(shaderPipelineTemplate.m_DescriptorSetLayouts)
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
        for (auto& descriptor : set.Descriptors)
        {
            shaderPipelineTemplate.m_DescriptorsInfo.push_back({
                .Name = descriptor.Name,
                .Set = set.Set,
                .Binding = descriptor.Descriptor.Binding,
                .Type = descriptor.Descriptor.Type,
                .ShaderStages = descriptor.Descriptor.Shaders,
                .Flags = descriptor.Flags 
            });
        }

        shaderPipelineTemplate.m_DescriptorSetFlags.push_back(set.LayoutFlags);
        shaderPipelineTemplate.m_DescriptorPoolFlags.push_back(set.PoolFlags);
    }
        
    shaderPipelineTemplate.m_DescriptorSetCount = (u32)reflectionData.DescriptorSets.size();
    
    return shaderPipelineTemplate;
}

void ShaderPipelineTemplate::Destroy(const ShaderPipelineTemplate& shaderPipelineTemplate)
{
    for (auto& shader : shaderPipelineTemplate.m_Shaders)
        ShaderModule::Destroy(shader);
}

const ShaderPipelineTemplate::DescriptorInfo& ShaderPipelineTemplate::GetDescriptorInfo(std::string_view name)
{
    for (auto& binding : m_DescriptorsInfo)
        if (binding.Name == name)
            return binding;
    
    ASSERT(false, "Unrecognized descriptor binding name")
    std::unreachable();
}

bool ShaderPipelineTemplate::IsComputeTemplate() const
{
    return m_Shaders.size() == 1 && m_Shaders.front().m_Stage == ShaderStage::Compute;
}

std::vector<DescriptorSetLayout> ShaderPipelineTemplate::CreateDescriptorLayouts(
    const std::vector<ReflectionData::DescriptorSet>& descriptorSetReflections)
{
    std::vector<DescriptorSetLayout> layouts;
    layouts.reserve(descriptorSetReflections.size());
    for (auto& set : descriptorSetReflections)
    {
        DescriptorsFlags descriptorsFlags = ExtractDescriptorsAndFlags(set);

        DescriptorSetLayout layout = DescriptorSetLayout::Builder()
            .SetBindings(descriptorsFlags.Descriptors)
            .SetBindingFlags(descriptorsFlags.Flags)
            .SetFlags(set.LayoutFlags)
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
    const ReflectionData::DescriptorSet& descriptorSet)
{
    DescriptorsFlags descriptorsFlags;
    descriptorsFlags.Descriptors.reserve(descriptorSet.Descriptors.size());
    descriptorsFlags.Flags.reserve(descriptorSet.Descriptors.size());
    
    for (auto& descriptor : descriptorSet.Descriptors)
    {
        descriptorsFlags.Descriptors.push_back(descriptor.Descriptor);
        descriptorsFlags.Flags.push_back(descriptor.Flags);
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

void ShaderPipeline::Builder::Prebuild()
{
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
    m_CreateInfo.UsedSets = {};
    m_CreateInfo.DescriptorBuilders = {};

    return descriptorSet;
}

ShaderDescriptorSet ShaderDescriptorSet::Builder::BuildManualLifetime()
{
    PreBuild();
    for (auto& builder : m_CreateInfo.DescriptorBuilders)
        builder.SetPoolFlags(DescriptorPoolFlags::FreeSet);
    m_CreateInfo.ManualLifetime = true;
    ShaderDescriptorSet descriptorSet = ShaderDescriptorSet::Create(m_CreateInfo);
    m_CreateInfo.UsedSets = {};
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
    const DescriptorInfo& descriptorInfo = m_CreateInfo.ShaderPipelineTemplate->GetDescriptorInfo(name);
    m_CreateInfo.UsedSets[descriptorInfo.Set]++;

    m_CreateInfo.DescriptorBuilders[descriptorInfo.Set].AddBufferBinding(
        descriptorInfo.Binding,
        {
            .Buffer = &buffer,
            .SizeBytes = sizeBytes,
            .Offset = offset
        },
        descriptorInfo.Type);

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name,
    const DescriptorSet::TextureBindingInfo& texture)
{
    const DescriptorInfo& descriptorInfo = m_CreateInfo.ShaderPipelineTemplate->GetDescriptorInfo(name);
    m_CreateInfo.UsedSets[descriptorInfo.Set]++;

    m_CreateInfo.DescriptorBuilders[descriptorInfo.Set].AddTextureBinding(
        descriptorInfo.Binding,
        texture,
        descriptorInfo.Type);

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name, u32 variableBindingCount)
{
    const DescriptorInfo& descriptorInfo = m_CreateInfo.ShaderPipelineTemplate->GetDescriptorInfo(name);
    m_CreateInfo.UsedSets[descriptorInfo.Set]++;

    m_CreateInfo.DescriptorBuilders[descriptorInfo.Set].AddVariableBinding(
        {.Slot = descriptorInfo.Binding, .Count = variableBindingCount});

    return *this;
}

void ShaderDescriptorSet::Builder::PreBuild()
{
    u32 descriptorCount = m_CreateInfo.ShaderPipelineTemplate->m_DescriptorSetCount;
    for (u32 i = 0; i < descriptorCount; i++)
    {
        m_CreateInfo.DescriptorBuilders[i].SetAllocator(m_CreateInfo.ShaderPipelineTemplate->m_Allocator);
        if (m_CreateInfo.UsedSets[i] > 0)
        {
            m_CreateInfo.DescriptorBuilders[i].SetLayout(
                m_CreateInfo.ShaderPipelineTemplate->GetDescriptorSetLayout(i));
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
        if (createInfo.UsedSets[i] == 0)
            continue;
        
        descriptorSet.m_DescriptorSetsInfo.DescriptorSets[i].IsPresent = true;
        if (createInfo.ManualLifetime)
            descriptorSet.m_DescriptorSetsInfo.DescriptorSets[i].Set = const_cast<DescriptorSet::Builder&>(
                createInfo.DescriptorBuilders[i]).BuildManualLifetime();
        else
            descriptorSet.m_DescriptorSetsInfo.DescriptorSets[i].Set = const_cast<DescriptorSet::Builder&>(
                createInfo.DescriptorBuilders[i]).Build();
        setCount++;
    }
    descriptorSet.m_DescriptorSetsInfo.DescriptorCount = setCount;

    return descriptorSet;
}

void ShaderDescriptorSet::Destroy(const ShaderDescriptorSet& descriptorSet)
{
    for (auto& set : descriptorSet.m_DescriptorSetsInfo.DescriptorSets)
        if (set.IsPresent)
            DescriptorSet::Destroy(set.Set);
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
    const ShaderPipelineTemplate::DescriptorInfo& descriptorInfo = m_Template->GetDescriptorInfo(name);
    ASSERT(m_DescriptorSetsInfo.DescriptorSets[descriptorInfo.Set].IsPresent,
        "Attempt to access non-existing desriptor set")

    m_DescriptorSetsInfo.DescriptorSets[descriptorInfo.Set].Set.SetTexture(
        descriptorInfo.Binding,
        texture,
        descriptorInfo.Type,
        arrayIndex);
}

std::unordered_map<std::string, ShaderPipelineTemplate> ShaderTemplateLibrary::m_Templates = {};

ShaderPipelineTemplate* ShaderTemplateLibrary::LoadShaderPipelineTemplate(const std::vector<std::string_view>& paths,
    std::string_view templateName, DescriptorAllocator& allocator)
{
    if (!GetShaderTemplate(std::string{templateName}))
    {
        Shader* shaderReflection = Shader::ReflectFrom(paths);

        ShaderPipelineTemplate shaderTemplate = ShaderPipelineTemplate::Builder()
            .SetDescriptorAllocator(&allocator)
            .SetShaderReflection(shaderReflection)
            .Build();
        
        AddShaderTemplate(shaderTemplate, std::string{templateName});
    }
    
    return GetShaderTemplate(std::string{templateName});
}

ShaderPipelineTemplate* ShaderTemplateLibrary::GetShaderTemplate(const std::string& name)
{
    auto it = m_Templates.find(name);
    return it == m_Templates.end() ? nullptr : &it->second;
}

void ShaderTemplateLibrary::AddShaderTemplate(const ShaderPipelineTemplate& shaderTemplate, const std::string& name)
{
    m_Templates.emplace(std::make_pair(name, shaderTemplate));
}
