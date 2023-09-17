#include "Shader.h"

#include <spirv_reflect.h>

#include <fstream>

#include "Buffer.h"
#include "core.h"
#include "DescriptorSet.h"
#include "Driver.h"
#include "Pipeline.h"
#include "RenderCommand.h"
#include "utils.h"
#include "VulkanUtils.h"

void ShaderReflection::LoadFromAsset(std::string_view path)
{
    // load the spirv bytecode from file
    std::ifstream in(path.data(), std::ios::ate | std::ios::binary);
    usize sizeBytes = in.tellg();
    std::vector<u8> shaderSrc(sizeBytes);
    in.seekg(0);
    in.read((char*)shaderSrc.data(), (i64)sizeBytes);

    m_Modules.push_back({.Source = shaderSrc});
}

void ShaderReflection::ReflectFrom(const std::vector<std::string_view>& paths)
{
    for (auto& path : paths)
        LoadFromAsset(path);
    Reflect();
}

void ShaderReflection::Reflect()
{
    ReflectionData merged = {};
    for (auto& module : m_Modules)
    {
        ModuleReflectionData reflectionData = ReflectModule(module);
        module.Kind = vkUtils::shaderKindByStage(reflectionData.ShaderStages);
        merged = MergeReflections(merged, reflectionData);
    }
    m_ReflectionData = merged;
    ASSERT(m_ReflectionData.DescriptorSetReflections.size() <= MAX_PIPELINE_DESCRIPTOR_SETS,
        "Can have only {} different descriptor sets, but have {}",
        MAX_PIPELINE_DESCRIPTOR_SETS, m_ReflectionData.DescriptorSetReflections.size())
}

ShaderReflection::ModuleReflectionData ShaderReflection::ReflectModule(const ShaderModule& module)
{
    ModuleReflectionData moduleReflectionData = {};

    static constexpr u32 SPV_INVALID_VAL = (u32)-1;
    SpvReflectShaderModule reflectedModule = {};
    spvReflectCreateShaderModule(module.Source.size(), module.Source.data(), &reflectedModule);

    moduleReflectionData.ShaderStages = (VkShaderStageFlags)reflectedModule.shader_stage;

    // extract input attributes
    if (reflectedModule.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
    {
        u32 inputCount;
        spvReflectEnumerateInputVariables(&reflectedModule, &inputCount, nullptr);
        std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
        spvReflectEnumerateInputVariables(&reflectedModule, &inputCount, inputs.data());

        moduleReflectionData.InputAttributeReflections.reserve(inputCount);
        for (auto& input : inputs)
        {
            if (input->location == SPV_INVALID_VAL)
                continue;
            moduleReflectionData.InputAttributeReflections.push_back({
                .Location = input->location,
                .Name = input->name,
                .Format = (VkFormat)input->format
            });
        }
        std::ranges::sort(moduleReflectionData.InputAttributeReflections,
                          [](const auto& a, const auto& b) { return a.Location < b.Location; });
    }

    // extract push constants
    u32 pushCount;
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, nullptr);
    std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
    spvReflectEnumeratePushConstantBlocks(&reflectedModule, &pushCount, pushConstants.data());

    moduleReflectionData.PushConstantReflections.reserve(pushCount);
    for (auto& push : pushConstants)
        moduleReflectionData.PushConstantReflections.push_back({.SizeBytes = push->size, .Offset = push->offset, .ShaderStages = (VkShaderStageFlags)reflectedModule.shader_stage});
    std::ranges::sort(moduleReflectionData.PushConstantReflections,
                      [](const auto& a, const auto& b) { return a.Offset < b.Offset; });

    // extract descriptors
    u32 setCount;
    spvReflectEnumerateDescriptorSets(&reflectedModule, &setCount, nullptr);
    std::vector<SpvReflectDescriptorSet*> sets(setCount);
    spvReflectEnumerateDescriptorSets(&reflectedModule, &setCount, sets.data());

    ASSERT(setCount <= MAX_PIPELINE_DESCRIPTOR_SETS, "Can have only {} different descriptor sets, but have {}", MAX_PIPELINE_DESCRIPTOR_SETS, setCount)
    moduleReflectionData.DescriptorSetReflections.reserve(setCount);
    for (auto& set : sets)
    {
        moduleReflectionData.DescriptorSetReflections.push_back({.Set = set->set});
        DescriptorSetReflection& descriptorSet = moduleReflectionData.DescriptorSetReflections.back();
        descriptorSet.Bindings.resize(set->binding_count);
        for (u32 i = 0; i < set->binding_count; i++)
        {
            descriptorSet.Bindings[i] = {
                .Binding = set->bindings[i]->binding,
                .Name = set->bindings[i]->name,
                .Descriptor = (VkDescriptorType)set->bindings[i]->descriptor_type,
                .ShaderStages = (VkShaderStageFlags)reflectedModule.shader_stage
            };
            if (descriptorSet.Bindings[i].Name.starts_with("dyn"))
            {
                if (descriptorSet.Bindings[i].Descriptor == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    descriptorSet.Bindings[i].Descriptor = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                else if (descriptorSet.Bindings[i].Descriptor == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                    descriptorSet.Bindings[i].Descriptor = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
            }
        }
        std::ranges::sort(descriptorSet.Bindings,
                          [](const auto& a, const auto& b) { return a.Binding < b.Binding; });
    }
    std::ranges::sort(moduleReflectionData.DescriptorSetReflections,
                      [](const auto& a, const auto& b) { return a.Set < b.Set; });

    return moduleReflectionData;
}

ShaderReflection::ReflectionData ShaderReflection::MergeReflections(
    const ModuleReflectionData& first, const ModuleReflectionData& second)
{
    ASSERT(!(first.ShaderStages & second.ShaderStages), "Overlapping shader stages")

    ReflectionData merged = first;

    merged.ShaderStages |= second.ShaderStages;

    // merge inputs (possibly nothing happens)
    merged.InputAttributeReflections.append_range(second.InputAttributeReflections);

    // merge push constants
    merged.PushConstantReflections = utils::mergeSets(merged.PushConstantReflections, second.PushConstantReflections,
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
            PushConstantReflection merged = a;
            merged.ShaderStages |= b.ShaderStages;
            return merged;
        });

    // merge descriptor sets
    merged.DescriptorSetReflections = utils::mergeSets(merged.DescriptorSetReflections, second.DescriptorSetReflections,
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
            DescriptorSetReflection mergedSet = a;
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
                    DescriptorSetReflection::DescriptorBindingReflection mergedDescriptor = a;
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

ShaderPipelineTemplate::Builder& ShaderPipelineTemplate::Builder::SetShaderReflection(ShaderReflection* shaderReflection)
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
    
    const ShaderReflection::ReflectionData& reflectionData = createInfo.ShaderReflection->GetReflectionData();
    
    std::vector<DescriptorSetLayout*> layouts = CreateDescriptorLayouts(reflectionData.DescriptorSetReflections, createInfo.LayoutCache);
    shaderPipelineTemplate.m_VertexInputDescription = CreateInputDescription(reflectionData.InputAttributeReflections);
    std::vector<PushConstantDescription> pushConstantDescriptions = CreatePushConstantDescriptions(reflectionData.PushConstantReflections);
    std::vector<ShaderModuleData> shaderModules = CreateShaderModules(createInfo.ShaderReflection->GetShaders());

    shaderPipelineTemplate.m_PipelineLayout = PipelineLayout::Builder().
       SetPushConstants(pushConstantDescriptions).
       SetDescriptorLayouts(layouts).
       Build();
    
    shaderPipelineTemplate.m_PipelineBuilder = Pipeline::Builder().
        FixedFunctionDefaults().
        SetLayout(shaderPipelineTemplate.m_PipelineLayout);

    shaderPipelineTemplate.m_Shaders.reserve(shaderModules.size());
    for (auto& shader : shaderModules)
    {
        shaderPipelineTemplate.m_PipelineBuilder.AddShader(shader);
        shaderPipelineTemplate.m_Shaders.push_back(shader);
    }

    for (auto& set : reflectionData.DescriptorSetReflections)
        for (auto& descriptor : set.Bindings)
            shaderPipelineTemplate.m_DescriptorsInfo.push_back({
                .Name = descriptor.Name,
                .Set = set.Set,
                .Binding = descriptor.Binding,
                .Descriptor = descriptor.Descriptor,
                .ShaderStages = descriptor.ShaderStages});

    shaderPipelineTemplate.m_DescriptorSetCount = (u32)reflectionData.DescriptorSetReflections.size();
    
    return shaderPipelineTemplate;
}

void ShaderPipelineTemplate::Destroy(const ShaderPipelineTemplate& shaderPipelineTemplate)
{
    for (auto& shader : shaderPipelineTemplate.m_Shaders)
        vkDestroyShaderModule(Driver::DeviceHandle(), shader.Module, nullptr);
}

std::vector<DescriptorSetLayout*> ShaderPipelineTemplate::CreateDescriptorLayouts(
    const std::vector<ShaderReflection::DescriptorSetReflection>& descriptorSetReflections,
    DescriptorLayoutCache* layoutCache)
{
    std::vector<DescriptorSetLayout*> layouts;
    layouts.reserve(descriptorSetReflections.size());
    for (auto& set : descriptorSetReflections)
        layouts.push_back(layoutCache->CreateDescriptorSetLayout(ExtractBindings(set)));

    return layouts;
}

VertexInputDescription ShaderPipelineTemplate::CreateInputDescription(
    const std::vector<ShaderReflection::InputAttributeReflection>& inputAttributeReflections)
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

std::vector<PushConstantDescription> ShaderPipelineTemplate::CreatePushConstantDescriptions(
    const std::vector<ShaderReflection::PushConstantReflection>& pushConstantReflections)
{
    std::vector<PushConstantDescription> pushConstants;
    pushConstants.reserve(pushConstantReflections.size());

    for (auto& pushConstant : pushConstantReflections)
    {
        PushConstantDescription description = PushConstantDescription::Builder().
            SetSizeBytes(pushConstant.SizeBytes).
            SetOffset(pushConstant.Offset).
            SetStages(pushConstant.ShaderStages).
            Build();

        pushConstants.push_back(description);
    }

    return pushConstants;
}

std::vector<ShaderModuleData> ShaderPipelineTemplate::CreateShaderModules(const std::vector<ShaderReflection::ShaderModule>& shaders)
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

std::vector<VkDescriptorSetLayoutBinding> ShaderPipelineTemplate::ExtractBindings(
    const ShaderReflection::DescriptorSetReflection& descriptorSet)
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
    m_ComaptibleVertexDescription = vertexInputDescription;

    return *this;
}

void ShaderPipeline::Builder::Prebuild()
{
    // adapt vertex input layout
    const VertexInputDescription& available = m_CreateInfo.ShaderPipelineTemplate->m_VertexInputDescription;
    const VertexInputDescription& compatible = m_ComaptibleVertexDescription;
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

    shaderPipeline.m_Pipeline = shaderPipeline.m_Template->m_PipelineBuilder.
        SetRenderPass(*createInfo.RenderPass).
        Build();

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

void ShaderDescriptorSet::Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind,
    const ShaderPipeline& pipeline, VkPipelineBindPoint bindPoint)
{
    RenderCommand::BindDescriptorSet(commandBuffer, GetDescriptorSet(descriptorKind), pipeline.GetPipelineLayout(), (u32)descriptorKind, bindPoint, {});
}

void ShaderDescriptorSet::Bind(const CommandBuffer& commandBuffer, DescriptorKind descriptorKind,
    const ShaderPipeline& pipeline, VkPipelineBindPoint bindPoint, const std::vector<u32>& dynamicOffsets)
{
    RenderCommand::BindDescriptorSet(commandBuffer, GetDescriptorSet(descriptorKind), pipeline.GetPipelineLayout(), (u32)descriptorKind, bindPoint, dynamicOffsets);
}
