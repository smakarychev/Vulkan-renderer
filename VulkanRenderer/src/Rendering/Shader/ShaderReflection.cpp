#include "ShaderReflection.h"

#include "AssetManager.h"
#include "ShaderPipelineTemplate.h"
#include "Vulkan/Device.h"
#include "Utils/ContainterUtils.h"
#include "Rendering/DeletionQueue.h"

#include <spirv_reflect.h>

#include "ShaderAsset.h"

namespace
{
    // this function is pretty questionable, but theoretically we might use some other format for assets
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
            ASSERT(false, "Unsupported shader kind")
        }
        std::unreachable();
    }

    // this function is pretty questionable, but theoretically we might use some other format for assets
    ShaderStage shaderStageFromMultipleAssetStages(u32 assetStage)
    {
        u32 typedStage = (SpvReflectShaderStageFlagBits)assetStage;
        ShaderStage stage = ShaderStage::None;
        if ((typedStage & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT) != 0)
            stage |= ShaderStage::Vertex;
        if ((typedStage & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT) != 0)
            stage |= ShaderStage::Pixel;
        if ((typedStage & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) != 0)
            stage |= ShaderStage::Compute;

        return stage;
    }

    // this function is pretty questionable, but theoretically we might use some other format for assets
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
        /* divide max count by 2 to allow for non-bindless resources in pipeline */
        switch ((SpvReflectDescriptorType)descriptorType)
        {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            return Device::GetMaxIndexingImages() >> 1;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            return Device::GetMaxIndexingUniformBuffers() >> 1;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            return Device::GetMaxIndexingStorageBuffers() >> 1;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            return Device::GetMaxIndexingUniformBuffersDynamic() >> 1;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            return Device::GetMaxIndexingStorageBuffersDynamic() >> 1;
        default:
            ASSERT(false, "Unsupported descriptor bindless type")
            break;
        }
        std::unreachable();
    }

    assetLib::ShaderStageInfo mergeReflections(const assetLib::ShaderStageInfo& first,
        const assetLib::ShaderStageInfo& second)
    {
        ASSERT(!(first.ShaderStages & second.ShaderStages), "Overlapping shader stages")
        ASSERT(((first.ShaderStages | second.ShaderStages) & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) == 0 ||
               ((first.ShaderStages | second.ShaderStages) & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT) ==
               SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT,
               "Compute shaders cannot be combined with others in pipeline")

        assetLib::ShaderStageInfo merged = first;

        merged.ShaderStages |= second.ShaderStages;

        // merge specialization constants
        merged.SpecializationConstants = Utils::mergeSets(merged.SpecializationConstants, second.SpecializationConstants,
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
                assetLib::ShaderStageInfo::SpecializationConstant merged = a;
                merged.ShaderStages |= b.ShaderStages;
                return merged;
            });
        
        // merge inputs (possibly nothing happens)
        merged.InputAttributes.append_range(second.InputAttributes);

        // merge push constants
        merged.PushConstants = Utils::mergeSets(merged.PushConstants, second.PushConstants,
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
                assetLib::ShaderStageInfo::PushConstant merged = a;
                merged.ShaderStages |= b.ShaderStages;
                return merged;
            });

        // merge descriptor sets
        merged.DescriptorSets = Utils::mergeSets(merged.DescriptorSets, second.DescriptorSets,
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
                assetLib::ShaderStageInfo::DescriptorSet mergedSet = a;
                mergedSet.Descriptors = Utils::mergeSets(mergedSet.Descriptors, b.Descriptors,
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
                        assetLib::ShaderStageInfo::DescriptorSet::DescriptorBinding mergedDescriptor = a;
                        mergedDescriptor.ShaderStages |= b.ShaderStages;

                        return mergedDescriptor;
                   });

                return mergedSet;
            });

        merged.IncludedFiles.push_back(second.OriginalFile);
        for (auto& include : second.IncludedFiles)
        {
            auto it = std::ranges::find(merged.IncludedFiles, include);
            if (it == merged.IncludedFiles.end())
                merged.IncludedFiles.push_back(include);
        }

        return merged;
    }

    Sampler getImmutableSampler(ImageFilter filter, SamplerWrapMode wrapMode, SamplerBorderColor borderColor)
    {
        Sampler sampler = Device::CreateSampler({
            .MinificationFilter = filter,
            .MagnificationFilter = filter,
            .WrapMode = wrapMode,
            .BorderColor = borderColor});

        return sampler;
    }
    Sampler getImmutableShadowSampler(ImageFilter filter, SamplerDepthCompareMode depthCompareMode)
    {
        Sampler sampler = Device::CreateSampler({
            .MinificationFilter = filter,
            .MagnificationFilter = filter,
            .WrapMode = SamplerWrapMode::ClampBorder,
            .BorderColor = SamplerBorderColor::Black,
            .DepthCompareMode = depthCompareMode,
            .WithAnisotropy = false});

        return sampler;
    }

    std::array<ShaderReflection::DescriptorsInfo, MAX_DESCRIPTOR_SETS> processDescriptorSets(
        const std::vector<assetLib::ShaderStageInfo::DescriptorSet>& sets)
    {
        static SamplerBorderColor black = SamplerBorderColor::Black;
        static SamplerBorderColor white = SamplerBorderColor::White;
        static Sampler immutableSampler = getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::Repeat, black);
        static Sampler immutableSamplerNearest = getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::Repeat, black);
        static Sampler immutableSamplerClampEdge =
            getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::ClampEdge, black);
        static Sampler immutableSamplerNearestClampEdge =
            getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::ClampEdge, black);
        static Sampler immutableSamplerClampBlack =
            getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::ClampBorder, black);
        static Sampler immutableSamplerNearestClampBlack =
            getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::ClampBorder, black);
        static Sampler immutableSamplerClampWhite =
            getImmutableSampler(ImageFilter::Linear, SamplerWrapMode::ClampBorder, white);
        static Sampler immutableSamplerNearestClampWhite =
            getImmutableSampler(ImageFilter::Nearest, SamplerWrapMode::ClampBorder, white);
    
        static Sampler immutableShadowSampler =
            getImmutableShadowSampler(ImageFilter::Linear, SamplerDepthCompareMode::Less); 
        static Sampler immutableShadowNearestSampler =
            getImmutableShadowSampler(ImageFilter::Nearest, SamplerDepthCompareMode::Less);
        
        std::array<ShaderReflection::DescriptorsInfo, MAX_DESCRIPTOR_SETS> descriptorSets;
        for (auto& set : sets)
        {
            auto& target = descriptorSets[set.Set];
            target.Descriptors.resize(set.Descriptors.size());
            target.DescriptorNames.resize(set.Descriptors.size());
            bool containsBindlessDescriptors = false;
            for (u32 descriptorIndex = 0; descriptorIndex < set.Descriptors.size(); descriptorIndex++)
            {
                auto& name = target.DescriptorNames[descriptorIndex];
                auto& descriptor = target.Descriptors[descriptorIndex];
                descriptor.Binding = set.Descriptors[descriptorIndex].Binding;
                name = set.Descriptors[descriptorIndex].Name;
                descriptor.Type = descriptorTypeFromAssetDescriptorType(
                    set.Descriptors[descriptorIndex].Type);
                descriptor.Shaders = shaderStageFromMultipleAssetStages(
                    set.Descriptors[descriptorIndex].ShaderStages);

                auto flags = set.Descriptors[descriptorIndex].Flags;
                if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::Bindless))
                    descriptor.Flags |= DescriptorFlags::VariableCount;

                if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::Bindless))
                {
                    containsBindlessDescriptors = true;
                    descriptor.Count = bindlessDescriptorCount(
                        set.Descriptors[descriptorIndex].Type);
                }
                else
                {
                    descriptor.Count = set.Descriptors[descriptorIndex].Count;
                }

                if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerClampEdge))
                    descriptor.ImmutableSampler = immutableSamplerClampEdge;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearestClampEdge))
                    descriptor.ImmutableSampler = immutableSamplerNearestClampEdge;
                else if (enumHasAny(flags,assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerClampBlack))
                    descriptor.ImmutableSampler = immutableSamplerClampBlack;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearestClampBlack))
                    descriptor.ImmutableSampler = immutableSamplerNearestClampBlack;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerClampWhite))
                    descriptor.ImmutableSampler = immutableSamplerClampWhite;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearestClampWhite))
                    descriptor.ImmutableSampler = immutableSamplerNearestClampWhite;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerNearest))
                    descriptor.ImmutableSampler = immutableSamplerNearest;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerShadow))
                    descriptor.ImmutableSampler = immutableShadowSampler;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSamplerShadowNearest))
                    descriptor.ImmutableSampler = immutableShadowNearestSampler;
                else if (enumHasAny(flags, assetLib::ShaderStageInfo::DescriptorSet::ImmutableSampler))
                    descriptor.ImmutableSampler = immutableSampler;
            }
            target.HasBindless = containsBindlessDescriptors;
        }

        return descriptorSets;
    }

    VertexInputDescription processInputDescription(
        const std::vector<assetLib::ShaderStageInfo::InputAttribute>& inputAttributeReflections)
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

    std::vector<PushConstantDescription> processPushConstantDescriptions(
        const std::vector<assetLib::ShaderStageInfo::PushConstant>& pushConstantReflections)
    {
        std::vector<PushConstantDescription> pushConstants;
        pushConstants.reserve(pushConstantReflections.size());

        for (auto& pushConstant : pushConstantReflections)
        {
            PushConstantDescription description = {};
            description.SizeBytes = pushConstant.SizeBytes;
            description.Offset = pushConstant.Offset;
            description.StageFlags = shaderStageFromMultipleAssetStages(pushConstant.ShaderStages);

            pushConstants.push_back(description);
        }

        return pushConstants;
    }

    void mergeAliasedDescriptors(std::vector<assetLib::ShaderStageInfo::DescriptorSet::DescriptorBinding>& descriptors)
    {
        if (descriptors.empty())
            return;
        
        /* rely on a fact that descriptors are sorted at this stage */
        for (u32 i = (u32)descriptors.size() - 1; i >= 1; i--)
            if (descriptors[i].Binding == descriptors[i - 1].Binding)
                descriptors.erase(descriptors.begin() + i);
    }

    std::pair<assetLib::ShaderStageInfo, ShaderModule> loadFromAsset(std::string_view path)
    {
        assetLib::File shaderFile;
        assetLib::loadAssetFile(path, shaderFile);
        assetLib::ShaderStageInfo shaderInfo = assetLib::readShaderStageInfo(shaderFile);

        std::vector<std::byte> source;
        source.resize(shaderInfo.SourceSizeBytes);
        assetLib::unpackShaderStage(shaderInfo, shaderFile.Blob.data(), shaderFile.Blob.size(), source.data());
    
        const ShaderModule shaderModule = Device::CreateShaderModule({
            .Source = source,
            .Stage = shaderStageFromAssetStage(shaderInfo.ShaderStages)},
            Device::DummyDeletionQueue());

        return std::make_pair(shaderInfo, shaderModule);
    }
}


ShaderReflection ShaderReflection::ReflectFrom(const std::vector<std::string>& paths)
{
    ShaderReflection shader;

    ShaderStage allStages = ShaderStage::None;
    assetLib::ShaderStageInfo mergedShaderInfo = {};
    
    for (auto& path : paths)
    {
        auto&& [asset, module] = loadFromAsset(path);
        shader.m_Modules.push_back(module);
        allStages |= shaderStageFromAssetStage(asset.ShaderStages);
        mergedShaderInfo = mergeReflections(mergedShaderInfo, asset);
    }

    std::ranges::sort(mergedShaderInfo.DescriptorSets, [](auto& a, auto& b) { return a.Set < b.Set; });
    for (auto& set : mergedShaderInfo.DescriptorSets)
    {
        std::ranges::sort(set.Descriptors, [](auto& a, auto& b) { return a.Binding < b.Binding; });
        mergeAliasedDescriptors(set.Descriptors);

        // assert that bindings starts with 0 and have no holes
        for (u32 bindingIndex = 0; bindingIndex < set.Descriptors.size(); bindingIndex++)
            ASSERT(set.Descriptors[bindingIndex].Binding == bindingIndex,
                "Descriptor with index {} must have a binding of {}", bindingIndex, bindingIndex)
    }

    ASSERT(mergedShaderInfo.DescriptorSets.size() <= MAX_DESCRIPTOR_SETS,
        "Can have only {} different descriptor sets, but have {}",
        MAX_DESCRIPTOR_SETS, mergedShaderInfo.DescriptorSets.size())

    ASSERT(mergedShaderInfo.PushConstants.size() <= 1, "Only one push constant is supported")

    std::vector<SpecializationConstant> specializationConstants;
    specializationConstants.reserve(mergedShaderInfo.SpecializationConstants.size());
    for (auto& spec : mergedShaderInfo.SpecializationConstants)
        specializationConstants.push_back({
            .Name = spec.Name,
            .Id = spec.Id,
            .ShaderStages = shaderStageFromMultipleAssetStages(spec.ShaderStages)});

    shader.m_ShaderStages = allStages;
    shader.m_SpecializationConstants = specializationConstants;
    shader.m_VertexInputDescription = processInputDescription(mergedShaderInfo.InputAttributes);
    shader.m_PushConstants = processPushConstantDescriptions(mergedShaderInfo.PushConstants);
    shader.m_DescriptorSets = processDescriptorSets(mergedShaderInfo.DescriptorSets);

    return shader;
}

ShaderReflection::ShaderReflection(ShaderReflection&& other) noexcept
{
    for (auto module : m_Modules)
        Device::DeletionQueue().Enqueue(module);
    m_ShaderStages = std::move(other.m_ShaderStages);
    m_SpecializationConstants = std::move(other.m_SpecializationConstants);
    m_VertexInputDescription = std::move(other.m_VertexInputDescription);
    m_PushConstants = std::move(other.m_PushConstants);
    m_DescriptorSets = std::move(other.m_DescriptorSets);
    m_Modules = std::move(other.m_Modules);
}

ShaderReflection& ShaderReflection::operator=(ShaderReflection&& other) noexcept
{
    if (this == &other)
        return *this;
    
    for (auto module : m_Modules)
        Device::DeletionQueue().Enqueue(module);
    m_ShaderStages = std::move(other.m_ShaderStages);
    m_SpecializationConstants = std::move(other.m_SpecializationConstants);
    m_VertexInputDescription = std::move(other.m_VertexInputDescription);
    m_PushConstants = std::move(other.m_PushConstants);
    m_DescriptorSets = std::move(other.m_DescriptorSets);
    m_Modules = std::move(other.m_Modules);
    
    return *this;
}

ShaderReflection::~ShaderReflection()
{
    for (auto module : m_Modules)
        Device::DeletionQueue().Enqueue(module);
}
