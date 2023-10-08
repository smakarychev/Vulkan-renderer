#include "Shader.h"

#include <spirv_reflect.h>

#include <fstream>

#include "AssetLib.h"
#include "Buffer.h"
#include "Core/core.h"
#include "DescriptorSet.h"
#include "Driver.h"
#include "Pipeline.h"
#include "RenderCommand.h"
#include "VulkanUtils.h"
#include "utils/utils.h"

void Shader::LoadFromAsset(std::string_view path)
{
    assetLib::File shaderFile;
    assetLib::loadAssetFile(path, shaderFile);
    assetLib::ShaderInfo shaderInfo = assetLib::readShaderInfo(shaderFile);

    m_ReflectionData = MergeReflections(m_ReflectionData, shaderInfo);
    ShaderModule shaderModule = {};
    shaderModule.Kind = vkUtils::shaderKindByStage(shaderInfo.ShaderStages);
    shaderModule.Source.resize(shaderInfo.SourceSizeBytes);
    assetLib::unpackShader(shaderInfo, shaderFile.Blob.data(), shaderFile.Blob.size(), shaderModule.Source.data());
    m_Modules.push_back(shaderModule);
    
    ASSERT(m_ReflectionData.DescriptorSets.size() <= MAX_PIPELINE_DESCRIPTOR_SETS,
        "Can have only {} different descriptor sets, but have {}",
        MAX_PIPELINE_DESCRIPTOR_SETS, m_ReflectionData.DescriptorSets.size())
}

void Shader::ReflectFrom(const std::vector<std::string_view>& paths)
{
    for (auto& path : paths)
        LoadFromAsset(path);
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
            mergedSet.Bindings = utils::mergeSets(
               mergedSet.Bindings, b.Bindings,
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
    
    std::vector<DescriptorSetLayout*> layouts = CreateDescriptorLayouts(reflectionData.DescriptorSets, createInfo.LayoutCache);
    shaderPipelineTemplate.m_VertexInputDescription = CreateInputDescription(reflectionData.InputAttributes);
    std::vector<PushConstantDescription> pushConstantDescriptions = CreatePushConstantDescriptions(reflectionData.PushConstants);
    std::vector<ShaderModuleData> shaderModules = CreateShaderModules(createInfo.ShaderReflection->GetShaders());

    shaderPipelineTemplate.m_PipelineLayout = PipelineLayout::Builder()
       .SetPushConstants(pushConstantDescriptions)
       .SetDescriptorLayouts(layouts)
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
        for (auto& descriptor : set.Bindings)
            shaderPipelineTemplate.m_DescriptorsInfo.push_back({
                .Name = descriptor.Name,
                .Set = set.Set,
                .Binding = descriptor.Binding,
                .Descriptor = descriptor.Descriptor,
                .ShaderStages = descriptor.ShaderStages});

    shaderPipelineTemplate.m_DescriptorSetCount = (u32)reflectionData.DescriptorSets.size();
    
    return shaderPipelineTemplate;
}

void ShaderPipelineTemplate::Destroy(const ShaderPipelineTemplate& shaderPipelineTemplate)
{
    for (auto& shader : shaderPipelineTemplate.m_Shaders)
        vkDestroyShaderModule(Driver::DeviceHandle(), shader.Module, nullptr);
}

bool ShaderPipelineTemplate::IsComputeTemplate() const
{
    return m_Shaders.size() == 1 && m_Shaders.front().Kind == ShaderKind::Compute;
}

std::vector<DescriptorSetLayout*> ShaderPipelineTemplate::CreateDescriptorLayouts(
    const std::vector<Shader::ShaderReflection::DescriptorSet>& descriptorSetReflections,
    DescriptorLayoutCache* layoutCache)
{
    std::vector<DescriptorSetLayout*> layouts;
    layouts.reserve(descriptorSetReflections.size());
    for (auto& set : descriptorSetReflections)
        layouts.push_back(layoutCache->CreateDescriptorSetLayout(ExtractBindings(set)));

    return layouts;
}

VertexInputDescription ShaderPipelineTemplate::CreateInputDescription(const std::vector<Shader::ShaderReflection::InputAttribute>&  inputAttributeReflections)
{
    VertexInputDescription inputDescription = {};
    inputDescription.Bindings.reserve(1);
    inputDescription.Attributes.reserve(inputAttributeReflections.size());

    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding.stride = 0;

    
    for (auto& input : inputAttributeReflections)
    {
        VkVertexInputAttributeDescription attributeDescription = {};
        attributeDescription.binding = 0;
        attributeDescription.format = input.Format;
        attributeDescription.location = input.Location;
        attributeDescription.offset = binding.stride;
        binding.stride += vkUtils::formatSizeBytes(input.Format);

        inputDescription.Attributes.push_back(attributeDescription);
    }
    inputDescription.Bindings.push_back(binding);

    return inputDescription;
}

std::vector<PushConstantDescription> ShaderPipelineTemplate::CreatePushConstantDescriptions(const std::vector<Shader::ShaderReflection::PushConstant>& pushConstantReflections)
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

std::vector<VkDescriptorSetLayoutBinding> ShaderPipelineTemplate::ExtractBindings(const Shader::ShaderReflection::DescriptorSet& descriptorSet)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(descriptorSet.Bindings.size());

    for (auto& binding : descriptorSet.Bindings)
    {
        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
        descriptorSetLayoutBinding.binding = binding.Binding;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.descriptorType = binding.Descriptor;
        descriptorSetLayoutBinding.stageFlags = binding.ShaderStages;

        bindings.push_back(descriptorSetLayoutBinding);
    }

    return bindings;
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

ShaderPipeline::Builder& ShaderPipeline::Builder::SetRenderPass(const RenderPass& renderPass)
{
    m_CreateInfo.RenderPass = &renderPass;
    
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
        ASSERT(m_CreateInfo.RenderPass == nullptr, "Compute shader pipeline does not need renderpass")
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
            .SetRenderPass(*createInfo.RenderPass)
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
    const BindingInfo& bindingInfo = FindDescriptorSet(name);
    m_CreateInfo.UsedSets[bindingInfo.Set]++;

    m_CreateInfo.DescriptorBuilders[bindingInfo.Set].AddBufferBinding(
        bindingInfo.Binding,
        {
            .Buffer = &buffer,
            .SizeBytes = sizeBytes,
            .OffsetBytes = offset
        },
        bindingInfo.Descriptor, bindingInfo.ShaderStages);

    return *this;
}

ShaderDescriptorSet::Builder& ShaderDescriptorSet::Builder::AddBinding(std::string_view name, const Texture& texture)
{
    const BindingInfo& bindingInfo = FindDescriptorSet(name);
    m_CreateInfo.UsedSets[bindingInfo.Set]++;

    m_CreateInfo.DescriptorBuilders[bindingInfo.Set].AddTextureBinding(
        bindingInfo.Binding,
        texture,
        bindingInfo.Descriptor, bindingInfo.ShaderStages);

    return *this;
}

void ShaderDescriptorSet::Builder::PreBuild()
{
    u32 descriptorCount = m_CreateInfo.ShaderPipelineTemplate->m_DescriptorSetCount;
    for (u32 i = 0; i < descriptorCount; i++)
    {
        m_CreateInfo.DescriptorBuilders[i].SetAllocator(m_CreateInfo.ShaderPipelineTemplate->m_Allocator);
        m_CreateInfo.DescriptorBuilders[i].SetLayoutCache(m_CreateInfo.ShaderPipelineTemplate->m_LayoutCache);
    }
}

const ShaderDescriptorSet::Builder::BindingInfo& ShaderDescriptorSet::Builder::FindDescriptorSet(std::string_view name) const
{
    for (auto& binding : m_CreateInfo.ShaderPipelineTemplate->m_DescriptorsInfo)
        if (binding.Name == name)
            return binding;
    
    ASSERT(false, "Unrecogrnized descriptor binding name")
    std::unreachable();
}

ShaderDescriptorSet ShaderDescriptorSet::Create(const Builder::CreateInfo& createInfo)
{
    ShaderDescriptorSet descriptorSet = {};

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
