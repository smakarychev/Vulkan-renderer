#include "Shader.h"

#include <spirv_reflect.h>

#include <fstream>

#include "AssetLib.h"
#include "AssetManager.h"
#include "Buffer.h"
#include "Core/core.h"
#include "DescriptorSet.h"
#include "Driver.h"
#include "Pipeline.h"
#include "RenderCommand.h"
#include "VulkanUtils.h"
#include "utils/utils.h"


Shader* Shader::ReflectFrom(const std::vector<std::string_view>& paths)
{
    std::string combinedNames;
    for (auto& path : paths)
        combinedNames += std::string{path};

    Shader* cachedShader = AssetManager::GetShader(combinedNames);  
    if (cachedShader)
        return cachedShader;

    Shader shader;
    
    assetLib::ShaderInfo mergedShaderInfo = {};
    
    for (auto& path : paths)
        mergedShaderInfo = MergeReflections(mergedShaderInfo, shader.LoadFromAsset(path));

    ASSERT(mergedShaderInfo.DescriptorSets.size() <= MAX_PIPELINE_DESCRIPTOR_SETS,
        "Can have only {} different descriptor sets, but have {}",
        MAX_PIPELINE_DESCRIPTOR_SETS, mergedShaderInfo.DescriptorSets.size())

    ASSERT(mergedShaderInfo.PushConstants.size() <= 1, "Only one push constant is supported")
    
    shader.m_ReflectionData = {
        .ShaderStages = mergedShaderInfo.ShaderStages,
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
    
    ShaderModule shaderModule = {};
    shaderModule.Kind = vkUtils::shaderKindByStage(shaderInfo.ShaderStages);
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
            mergedSet.Descriptors = utils::mergeSets(
               mergedSet.Descriptors, b.Descriptors,
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

std::vector<Shader::ReflectionData::DescriptorSet> Shader::ProcessDescriptorSets(const std::vector<assetLib::ShaderInfo::DescriptorSet>& sets)
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
            descriptor.Binding = sets[setIndex].Descriptors[descriptorIndex].Binding;
            descriptor.Name = sets[setIndex].Descriptors[descriptorIndex].Name;
            descriptor.Type = sets[setIndex].Descriptors[descriptorIndex].Type;
            descriptor.ShaderStages = sets[setIndex].Descriptors[descriptorIndex].ShaderStages;

            if (sets[setIndex].Descriptors[descriptorIndex].Flags & assetLib::ShaderInfo::DescriptorSet::Bindless)
            {
                containsBindlessDescriptors = true;
                descriptor.Count = GetBindlessDescriptorCount(descriptor.Type);
                descriptor.Flags =
                    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                    VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
            }
            else
            {
                descriptor.Count = 1;
            }
            
            descriptor.IsImmutableSampler = sets[setIndex].Descriptors[descriptorIndex].Flags &
                assetLib::ShaderInfo::DescriptorSet::ImmutableSampler;
        }
        if (containsBindlessDescriptors)
        {
            descriptorSets[setIndex].LayoutFlags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
            descriptorSets[setIndex].PoolFlags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        }
    }

    return descriptorSets;
}

u32 Shader::GetBindlessDescriptorCount(VkDescriptorType type)
{
    switch (type)
    {
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:              
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:     return Driver::GetMaxIndexingImages();
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:             return Driver::GetMaxIndexingUniformBuffers();
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:             return Driver::GetMaxIndexingStorageBuffers();
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:     return Driver::GetMaxIndexingUniformBuffersDynamic();
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:     return Driver::GetMaxIndexingStorageBuffersDynamic();
    default:
        LOG("Unsupported descriptor bindless type");
        return 0;
    }
}

ShaderPipelineTemplate ShaderPipelineTemplate::Builder::Build()
{
    ShaderPipelineTemplate shaderPipelineTemplate = ShaderPipelineTemplate::Create(m_CreateInfo);
    Driver::DeletionQueue().AddDeleter([shaderPipelineTemplate]() {ShaderPipelineTemplate::Destroy(shaderPipelineTemplate); });

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

ShaderPipelineTemplate::Builder& ShaderPipelineTemplate::Builder::SetDescriptorLayoutCache(DescriptorLayoutCache* layoutCache)
{
    m_CreateInfo.LayoutCache = layoutCache;

    return *this;
}

ShaderPipelineTemplate ShaderPipelineTemplate::Create(const Builder::CreateInfo& createInfo)
{
    ShaderPipelineTemplate shaderPipelineTemplate = {};

    shaderPipelineTemplate.m_Allocator = createInfo.Allocator;
    shaderPipelineTemplate.m_LayoutCache = createInfo.LayoutCache;
    
    const auto& reflectionData = createInfo.ShaderReflection->GetReflectionData();
    
    shaderPipelineTemplate.m_DescriptorSetLayouts = CreateDescriptorLayouts(reflectionData.DescriptorSets, createInfo.LayoutCache);
    shaderPipelineTemplate.m_VertexInputDescription = CreateInputDescription(reflectionData.InputAttributes);
    std::vector<PushConstantDescription> pushConstantDescriptions = CreatePushConstantDescriptions(reflectionData.PushConstants);
    if (!pushConstantDescriptions.empty())
        shaderPipelineTemplate.m_PushConstantDescription = pushConstantDescriptions.front();
    std::vector<ShaderModuleData> shaderModules = CreateShaderModules(createInfo.ShaderReflection->GetShaders());
    
    shaderPipelineTemplate.m_PipelineLayout = PipelineLayout::Builder()
       .SetPushConstants(pushConstantDescriptions)
       .SetDescriptorLayouts(shaderPipelineTemplate.m_DescriptorSetLayouts)
       .Build();
    
    shaderPipelineTemplate.m_PipelineBuilder = Pipeline::Builder()
        .FixedFunctionDefaults()
        .SetLayout(shaderPipelineTemplate.m_PipelineLayout);

    shaderPipelineTemplate.m_Shaders.reserve(shaderModules.size());
    for (auto& shader : shaderModules)
    {
        shaderPipelineTemplate.m_PipelineBuilder.AddShader(shader);
        shaderPipelineTemplate.m_Shaders.push_back(shader);
    }

    if (shaderModules.size() == 1 && shaderModules.front().Kind == ShaderKind::Compute)
        shaderPipelineTemplate.m_PipelineBuilder.IsComputePipeline(true);
    
    for (auto& set : reflectionData.DescriptorSets)
    {
        for (auto& descriptor : set.Descriptors)
        {
            shaderPipelineTemplate.m_DescriptorsInfo.push_back({
                .Name = descriptor.Name,
                .Set = set.Set,
                .Binding = descriptor.Binding,
                .Type = descriptor.Type,
                .ShaderStages = descriptor.ShaderStages,
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
        vkDestroyShaderModule(Driver::DeviceHandle(), shader.Module, nullptr);
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
    return m_Shaders.size() == 1 && m_Shaders.front().Kind == ShaderKind::Compute;
}

std::vector<DescriptorSetLayout*> ShaderPipelineTemplate::CreateDescriptorLayouts(
    const std::vector<ReflectionData::DescriptorSet>& descriptorSetReflections,
    DescriptorLayoutCache* layoutCache)
{
    std::vector<DescriptorSetLayout*> layouts;
    layouts.reserve(descriptorSetReflections.size());
    for (auto& set : descriptorSetReflections)
    {
        DescriptorsFlags descriptorsFlags = ExtractDescriptorsAndFlags(set);
        layouts.push_back(layoutCache->CreateDescriptorSetLayout(descriptorsFlags.Descriptors, descriptorsFlags.Flags, set.LayoutFlags));
    }

    return layouts;
}

VertexInputDescription ShaderPipelineTemplate::CreateInputDescription(const std::vector<ReflectionData::InputAttribute>& inputAttributeReflections)
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

    for (u32 i = 0; i < bindingsDense.size(); i++)
    {
        VkVertexInputBindingDescription binding = {};
        binding.binding = bindingsDense[i];
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        binding.stride = 0;
        inputDescription.Bindings.push_back(binding);
    }
    
    for (auto& input : inputAttributeReflections)
    {
        u32 bindingIndex = bindingsDense[bindingsSparse[input.Binding]];
        VkVertexInputBindingDescription& binding = inputDescription.Bindings[bindingIndex];
        VkVertexInputAttributeDescription attributeDescription = {};
        attributeDescription.binding = bindingIndex;
        attributeDescription.format = input.Format;
        attributeDescription.location = input.Location;
        attributeDescription.offset = binding.stride;
        binding.stride += vkUtils::formatSizeBytes(input.Format);

        inputDescription.Attributes.push_back(attributeDescription);
    }

    return inputDescription;
}

std::vector<PushConstantDescription> ShaderPipelineTemplate::CreatePushConstantDescriptions(const std::vector<ReflectionData::PushConstant>& pushConstantReflections)
{
    std::vector<PushConstantDescription> pushConstants;
    pushConstants.reserve(pushConstantReflections.size());

    for (auto& pushConstant : pushConstantReflections)
    {
        PushConstantDescription description = PushConstantDescription::Builder()
            .SetSizeBytes(pushConstant.SizeBytes)
            .SetOffset(pushConstant.Offset)
            .SetStages(pushConstant.ShaderStages)
            .Build();

        pushConstants.push_back(description);
    }

    return pushConstants;
}

std::vector<ShaderModuleData> ShaderPipelineTemplate::CreateShaderModules(const std::vector<Shader::ShaderModule>& shaders)
{
    std::vector<ShaderModuleData> shaderModules;
    shaderModules.reserve(shaders.size());

    for (auto& shader : shaders)
    {
        ShaderModuleData moduleData = {};
        moduleData.Kind = shader.Kind;

        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.codeSize = shader.Source.size();
        moduleCreateInfo.pCode = reinterpret_cast<const u32*>(shader.Source.data());

        VulkanCheck(vkCreateShaderModule(Driver::DeviceHandle(), &moduleCreateInfo, nullptr, &moduleData.Module),
            "Failed to create shader module");

        shaderModules.push_back(moduleData);
    }

    return shaderModules;
}

ShaderPipelineTemplate::DescriptorsFlags ShaderPipelineTemplate::ExtractDescriptorsAndFlags(const ReflectionData::DescriptorSet& descriptorSet)
{
    DescriptorsFlags descriptorsFlags;
    descriptorsFlags.Descriptors.reserve(descriptorSet.Descriptors.size());
    descriptorsFlags.Flags.reserve(descriptorSet.Descriptors.size());
    
    for (auto& descriptor : descriptorSet.Descriptors)
    {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
        descriptorSetLayoutBinding.binding = descriptor.Binding;
        descriptorSetLayoutBinding.descriptorCount = descriptor.Count;
        descriptorSetLayoutBinding.descriptorType = descriptor.Type;
        descriptorSetLayoutBinding.stageFlags = descriptor.ShaderStages;
        if (descriptor.IsImmutableSampler)
            descriptorSetLayoutBinding.pImmutableSamplers = Driver::GetImmutableSampler();
        descriptorsFlags.Descriptors.push_back(descriptorSetLayoutBinding);
        descriptorsFlags.Flags.push_back(descriptor.Flags);
    }

    return descriptorsFlags;
}

ShaderPipeline ShaderPipeline::Builder::Build()
{
    Prebuild();
    
    return ShaderPipeline::Create(m_CreateInfo);
}

ShaderPipeline::Builder& ShaderPipeline::Builder::PrimitiveKind(::PrimitiveKind primitiveKind)
{
    m_PrimitiveKind = primitiveKind;

    return *this;
}

ShaderPipeline::Builder& ShaderPipeline::Builder::SetRenderingDetails(const RenderingDetails& renderingDetails)
{
    m_CreateInfo.RenderingDetails = renderingDetails;
    
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
    
    if (!m_CompatibleVertexDescription.Bindings.empty())
        CreateCompatibleLayout();

    if (m_CreateInfo.ShaderPipelineTemplate->IsComputeTemplate())
        ASSERT(m_CreateInfo.RenderingDetails.ColorFormats.empty(), "Compute shader pipeline does not need rendering details")
}

void ShaderPipeline::Builder::CreateCompatibleLayout()
{
    // adapt vertex input layout
    const VertexInputDescription& available = m_CreateInfo.ShaderPipelineTemplate->m_VertexInputDescription;
    const VertexInputDescription& compatible = m_CompatibleVertexDescription;
    ASSERT(available.Bindings.size() == compatible.Bindings.size(), "Incompatible vertex inputs")
    
    VertexInputDescription adapted;
    adapted.Bindings = compatible.Bindings;
    adapted.Attributes.reserve(compatible.Attributes.size());

    for (u32 availI = 0; availI < available.Attributes.size(); availI++)
    {
        const auto& avail = available.Attributes[availI];
        for (u32 compI = availI; compI < compatible.Attributes.size(); compI++)
        {
            const auto& comp = compatible.Attributes[compI];
            if (comp.location == avail.location && comp.format == avail.format)
            {
                adapted.Attributes.push_back(comp);
                break;
            }
        }
        ASSERT(adapted.Attributes.size() == availI + 1, "Incompatible vertex inputs")
    }

    m_CreateInfo.ShaderPipelineTemplate->m_PipelineBuilder.SetVertexDescription(adapted);
}


ShaderPipeline ShaderPipeline::Create(const Builder::CreateInfo& createInfo)
{
    ShaderPipeline shaderPipeline = {};
    
    shaderPipeline.m_Template = createInfo.ShaderPipelineTemplate;

    if (createInfo.ShaderPipelineTemplate->IsComputeTemplate())
    {
        shaderPipeline.m_Pipeline = shaderPipeline.m_Template->m_PipelineBuilder
            .Build();
    }
    else
    {
        shaderPipeline.m_Pipeline = shaderPipeline.m_Template->m_PipelineBuilder
            .SetRenderingDetails(createInfo.RenderingDetails)
            .Build();    
    }

    return shaderPipeline;
}

void ShaderPipeline::Bind(const CommandBuffer& cmd, VkPipelineBindPoint bindPoint)
{
    m_Pipeline.Bind(cmd, bindPoint);
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
        builder.SetPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
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

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name, const Buffer& buffer, u64 sizeBytes, u64 offset)
{
    const DescriptorInfo& descriptorInfo = m_CreateInfo.ShaderPipelineTemplate->GetDescriptorInfo(name);
    m_CreateInfo.UsedSets[descriptorInfo.Set]++;

    m_CreateInfo.DescriptorBuilders[descriptorInfo.Set].AddBufferBinding(
        descriptorInfo.Binding,
        {
            .Buffer = &buffer,
            .SizeBytes = sizeBytes,
            .OffsetBytes = offset
        },
        descriptorInfo.Type);

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name, const Texture& texture)
{
    const DescriptorInfo& descriptorInfo = m_CreateInfo.ShaderPipelineTemplate->GetDescriptorInfo(name);
    m_CreateInfo.UsedSets[descriptorInfo.Set]++;

    m_CreateInfo.DescriptorBuilders[descriptorInfo.Set].AddTextureBinding(
        descriptorInfo.Binding,
        texture,
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
            m_CreateInfo.DescriptorBuilders[i].SetLayout(m_CreateInfo.ShaderPipelineTemplate->GetDescriptorSetLayout(i));
            m_CreateInfo.DescriptorBuilders[i].SetPoolFlags(m_CreateInfo.ShaderPipelineTemplate->m_DescriptorPoolFlags[i]);
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
        descriptorSet.m_DescriptorSetsInfo.DescriptorSets[i].Set = const_cast<DescriptorSet::Builder&>(createInfo.DescriptorBuilders[i]).Build();
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

void ShaderDescriptorSet::Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind,
                               const PipelineLayout& pipelineLayout, VkPipelineBindPoint bindPoint)
{
    RenderCommand::BindDescriptorSet(commandBuffer, GetDescriptorSet(descriptorKind), pipelineLayout, (u32)descriptorKind, bindPoint, {});
}

void ShaderDescriptorSet::Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind,
    const PipelineLayout& pipelineLayout, VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets)
{
    RenderCommand::BindDescriptorSet(commandBuffer, GetDescriptorSet(descriptorKind), pipelineLayout, (u32)descriptorKind, bindPoint, dynamicOffsets);
}

void ShaderDescriptorSet::SetTexture(std::string_view name, const Texture& texture, u32 arrayIndex)
{
    const ShaderPipelineTemplate::DescriptorInfo& descriptorInfo = m_Template->GetDescriptorInfo(name);
    ASSERT(m_DescriptorSetsInfo.DescriptorSets[descriptorInfo.Set].IsPresent, "Attempt to access non-existing desriptor set")

    m_DescriptorSetsInfo.DescriptorSets[descriptorInfo.Set].Set.SetTexture(
        descriptorInfo.Binding,
        texture,
        descriptorInfo.Type,
        arrayIndex);
}
