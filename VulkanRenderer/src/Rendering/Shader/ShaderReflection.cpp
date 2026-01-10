#include "rendererpch.h"

#include "ShaderReflection.h"

#include "AssetManager.h"
#include "ShaderPipelineTemplate.h"
#include "Vulkan/Device.h"
#include "Utils/ContainterUtils.h"
#include "Rendering/DeletionQueue.h"

#include "v2/Shaders/SlangShaderAsset.h"

namespace
{
Sampler getImmutableSampler(ImageFilter filter, SamplerWrapMode wrapMode, SamplerBorderColor borderColor)
{
    return Device::CreateSampler({
        .MinificationFilter = filter,
        .MagnificationFilter = filter,
        .WrapMode = wrapMode,
        .BorderColor = borderColor
    });
}

Sampler getImmutableShadowSampler(ImageFilter filter, SamplerDepthCompareMode depthCompareMode)
{
    return Device::CreateSampler({
        .MinificationFilter = filter,
        .MagnificationFilter = filter,
        .WrapMode = SamplerWrapMode::ClampBorder,
        .BorderColor = SamplerBorderColor::Black,
        .DepthCompareMode = depthCompareMode,
        .WithAnisotropy = false
    });
}

Sampler getImmutableReductionSampler(SamplerReductionMode reductionMode)
{
    static constexpr f32 MAX_LOD = 16.0f;
    return Device::CreateSampler({
        .ReductionMode = reductionMode,
        .MaxLod = MAX_LOD,
        .WithAnisotropy = false
    });
}

constexpr ShaderStage shaderStageFromAssetShaderStage(assetlib::ShaderStage stage)
{
    switch (stage)
    {
    case assetlib::ShaderStage::Vertex: return ShaderStage::Vertex;
    case assetlib::ShaderStage::Pixel: return ShaderStage::Pixel;
    case assetlib::ShaderStage::Compute: return ShaderStage::Compute;
    case assetlib::ShaderStage::None:
    default:
        ASSERT(false)
        return ShaderStage::None;
    }
}
constexpr ShaderStage shaderMultiStageFromAssetShaderMultiStage(assetlib::ShaderStage stages)
{
    ShaderStage stage = ShaderStage::None;
    if (enumHasAny(stages, assetlib::ShaderStage::Vertex))
        stage |= ShaderStage::Vertex;
    if (enumHasAny(stages, assetlib::ShaderStage::Pixel))
        stage |= ShaderStage::Pixel;
    if (enumHasAny(stages, assetlib::ShaderStage::Compute))
        stage |= ShaderStage::Compute;

    return stage;
}

constexpr Format formatFromAssetInputAttributeFormat(u32 elementCount, assetlib::ShaderScalarType scalar)
{
    switch (scalar)
    {
    case assetlib::ShaderScalarType::I32:
        switch (elementCount)
        {
        case 1: return Format::R32_SINT;
        case 2: return Format::RG32_SINT;
        case 3: return Format::RGB32_SINT;
        case 4: return Format::RGBA32_SINT;
        default:
            ASSERT(false)
            return Format::Undefined;
        }
    case assetlib::ShaderScalarType::U32:
        switch (elementCount)
        {
        case 1: return Format::R32_UINT;
        case 2: return Format::RG32_UINT;
        case 3: return Format::RGB32_UINT;
        case 4: return Format::RGBA32_UINT;
        default:
            ASSERT(false)
            return Format::Undefined;
        }
    case assetlib::ShaderScalarType::F32:
        switch (elementCount)
        {
        case 1: return Format::R32_FLOAT;
        case 2: return Format::RG32_FLOAT;
        case 3: return Format::RGB32_FLOAT;
        case 4: return Format::RGBA32_FLOAT;
        default:
            ASSERT(false)
            return Format::Undefined;
        }
    default:
        ASSERT(false)
        return Format::Undefined;
    }
}

constexpr u32 formatSizeBytesFromAssetInputAttributeFormat(u32 elementCount, assetlib::ShaderScalarType scalar)
{
    switch (scalar)
    {
    case assetlib::ShaderScalarType::I32:
    case assetlib::ShaderScalarType::U32:
    case assetlib::ShaderScalarType::F32:
        return sizeof(u32) * elementCount;
    default:
        ASSERT(false)
        return 0;
    }
}

::VertexInputDescription vertexInputDescriptionFromAssetInputAttribute(
    const std::vector<assetlib::ShaderInputAttribute>& attributes)
{
    VertexInputDescription inputs = {};

    inputs.Attributes.reserve(attributes.size());
    inputs.Bindings.reserve(attributes.size());
    for (auto& input : attributes)
    {
        if (!inputs.Bindings.empty() && inputs.Bindings.back().Index == input.Binding)
            continue;
        
        inputs.Bindings.push_back({
            .Index = input.Binding,
            .StrideBytes = 0
        });
    }
    for (auto& input : attributes)
    {
        auto bindingIt = std::ranges::find_if(inputs.Bindings, [&input](const auto& binding) {
            return binding.Index == input.Binding;
        });
        ASSERT(bindingIt != inputs.Bindings.end())

        inputs.Attributes.push_back({
            .Index = input.Location,
            .BindingIndex = input.Binding,
            .Format = formatFromAssetInputAttributeFormat(input.ElementCount, input.ElementScalar),
            .OffsetBytes = bindingIt->StrideBytes,
        });
        bindingIt->StrideBytes += formatSizeBytesFromAssetInputAttributeFormat(input.ElementCount, input.ElementScalar);
    }

    return inputs;
}

constexpr DescriptorType descriptorTypeFromAssetBindingType(assetlib::ShaderBindingType type)
{
    switch (type)
    {
    case assetlib::ShaderBindingType::Sampler: return DescriptorType::Sampler;
    case assetlib::ShaderBindingType::Image: return DescriptorType::Image;
    case assetlib::ShaderBindingType::ImageStorage: return DescriptorType::ImageStorage;
    case assetlib::ShaderBindingType::TexelUniform: return DescriptorType::TexelUniform;
    case assetlib::ShaderBindingType::TexelStorage: return DescriptorType::TexelStorage;
    case assetlib::ShaderBindingType::UniformBuffer: return DescriptorType::UniformBuffer;
    case assetlib::ShaderBindingType::StorageBuffer: return DescriptorType::StorageBuffer;
    case assetlib::ShaderBindingType::UniformTexelBuffer:
    case assetlib::ShaderBindingType::StorageTexelBuffer:
        ASSERT(false, "Texel buffers are not supported")
        return {};
    case assetlib::ShaderBindingType::UniformBufferDynamic: return DescriptorType::UniformBufferDynamic;
    case assetlib::ShaderBindingType::StorageBufferDynamic: return DescriptorType::StorageBufferDynamic;
    case assetlib::ShaderBindingType::Input: return DescriptorType::Input;
    default:
        ASSERT(false)
        return {};
    }

}

Sampler immutableSamplerFromAssetBindingAttributes(assetlib::ShaderBindingAttributes attributes)
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

    static Sampler immutableReductionMinSampler = getImmutableReductionSampler(SamplerReductionMode::Min);
    static Sampler immutableReductionMaxSampler = getImmutableReductionSampler(SamplerReductionMode::Max);

    using enum assetlib::ShaderBindingAttributes;
    if (!enumHasAny(attributes, ImmutableSampler))
        return {};

    if (enumHasAny(attributes, ImmutableSamplerNearest))
    {
        if (enumHasAny(attributes, ImmutableSamplerClampBlack))
            return immutableSamplerNearestClampBlack;
        if (enumHasAny(attributes, ImmutableSamplerClampWhite))
            return immutableSamplerNearestClampWhite;
        if (enumHasAny(attributes, ImmutableSamplerClampEdge))
            return immutableSamplerNearestClampEdge;
        if (enumHasAny(attributes, ImmutableSamplerShadow))
            return immutableShadowNearestSampler;
        return immutableSamplerNearest;
    }

    if (enumHasAny(attributes, ImmutableSamplerClampBlack))
        return immutableSamplerClampBlack;
    if (enumHasAny(attributes, ImmutableSamplerClampWhite))
        return immutableSamplerClampWhite;
    if (enumHasAny(attributes, ImmutableSamplerClampEdge))
        return immutableSamplerClampEdge;
    if (enumHasAny(attributes, ImmutableSamplerShadow))
        return immutableShadowSampler;

    if (enumHasAny(attributes, ImmutableSamplerReductionMin))
        return immutableReductionMinSampler;
    if (enumHasAny(attributes, ImmutableSamplerReductionMax))
        return immutableReductionMaxSampler;
    
    return immutableSampler;
}

constexpr u32 bindlessDescriptorCountForAssetBindingType(assetlib::ShaderBindingType type)
{
    /* divide max count by 2 to allow for non-bindless resources in pipeline */
    switch (type)
    {
    case assetlib::ShaderBindingType::Sampler:
    case assetlib::ShaderBindingType::Image:
    case assetlib::ShaderBindingType::ImageStorage:
        return Device::GetMaxIndexingImages() >> 1;
    case assetlib::ShaderBindingType::UniformBuffer:
        return Device::GetMaxIndexingUniformBuffers() >> 1;
    case assetlib::ShaderBindingType::StorageBuffer:
        return Device::GetMaxIndexingStorageBuffers() >> 1;
    case assetlib::ShaderBindingType::UniformBufferDynamic:
        return Device::GetMaxIndexingUniformBuffersDynamic() >> 1;
    case assetlib::ShaderBindingType::StorageBufferDynamic:
        return Device::GetMaxIndexingStorageBuffersDynamic() >> 1;
    default:
        ASSERT(false, "Unsupported descriptor bindless type")
        return 0;
    }
}

ShaderReflection::DescriptorSets descriptorSetsFromAssetBindingSets(const std::vector<assetlib::ShaderBindingSet>& sets)
{
    ShaderReflection::DescriptorSets descriptorSets = {};
    
    for (u32 i = 0; i < sets.size(); i++)
    {
        bool hasBindless = false;
        descriptorSets[i].Descriptors.reserve(sets[i].Bindings.size());
        for (auto& binding : sets[i].Bindings)
        {
            const bool bindingIsBindless = enumHasAny(binding.Attributes, assetlib::ShaderBindingAttributes::Bindless);
            hasBindless |= bindingIsBindless;
            descriptorSets[i].Descriptors.push_back({
                .Binding = binding.Binding,
                .Type = descriptorTypeFromAssetBindingType(binding.Type),
                .Count = bindingIsBindless ? bindlessDescriptorCountForAssetBindingType(binding.Type) : binding.Count,
                .Shaders = shaderMultiStageFromAssetShaderMultiStage(binding.ShaderStages),
                // todo: get rid of this, and just store attributes (store the entire shader asset in Reflection)
                .Flags = bindingIsBindless ? DescriptorFlags::VariableCount : DescriptorFlags::None,
                .ImmutableSampler = immutableSamplerFromAssetBindingAttributes(binding.Attributes)
            });
        }
        descriptorSets[i].HasBindless= hasBindless;
    }

    return descriptorSets;
}
}

assetlib::io::IoResult<ShaderReflection> ShaderReflection::Reflect(const std::filesystem::path& path)
{
    const auto assetFileResult = assetlib::io::loadAssetFile(path);
    if (!assetFileResult.has_value())
        return std::unexpected(assetFileResult.error());

    const assetlib::AssetFileAndBinary& assetFile = *assetFileResult;

    auto shaderResult = assetlib::shader::unpackHeader(assetFile.File);
    if (!shaderResult.has_value())
        return std::unexpected(shaderResult.error());

    auto spirvResult = assetlib::shader::unpackBinary(assetFile.File, assetFile.Binary);
    if (!spirvResult.has_value())
        return std::unexpected(spirvResult.error());
    
    assetlib::ShaderAsset shaderAsset = {};
    shaderAsset.Header = std::move(*shaderResult);
    shaderAsset.Spirv = std::move(*spirvResult);

    const auto& header = shaderAsset.Header;

    ShaderReflection shaderReflection = {};
    shaderReflection.m_Modules.push_back(Device::CreateShaderModule({
        .Source = shaderAsset.Spirv
    }, Device::DummyDeletionQueue()));

    for (auto& entry : header.EntryPoints)
        shaderReflection.m_ShaderStages |= shaderStageFromAssetShaderStage(entry.ShaderStage);

    if (header.PushConstant.SizeBytes > 0)
        shaderReflection.m_PushConstants.push_back({
            .SizeBytes = header.PushConstant.SizeBytes,
            .Offset = header.PushConstant.Offset,
            .StageFlags = shaderMultiStageFromAssetShaderMultiStage(header.PushConstant.ShaderStages)
        });

    // todo: spec also includes `type` I can use it as validation in ShaderOverrides
    shaderReflection.m_SpecializationConstants.reserve(header.SpecializationConstants.size());
    for (auto& spec : header.SpecializationConstants)
        shaderReflection.m_SpecializationConstants.push_back({
            .Name = spec.Name,
            .Id = spec.Id,
            .ShaderStages = shaderMultiStageFromAssetShaderMultiStage(spec.ShaderStages)
        });

    shaderReflection.m_VertexInputDescription = vertexInputDescriptionFromAssetInputAttribute(header.InputAttributes);
    
    if (header.BindingSets.size() > MAX_DESCRIPTOR_SETS)
        return std::unexpected(assetlib::io::IoError{
            .Code = assetlib::io::IoError::ErrorCode::WrongFormat,
            .Message = std::format("Shader has too many descriptor sets. Expected no more than {}, but got {}",
                MAX_DESCRIPTOR_SETS, header.BindingSets.size())
        });

    shaderReflection.m_DescriptorSets = descriptorSetsFromAssetBindingSets(header.BindingSets);

    return shaderReflection;
}

ShaderReflection::ShaderReflection(ShaderReflection&& other) noexcept
{
    for (auto module : m_Modules)
        Device::DeletionQueue().Enqueue(module);
    m_ShaderStages = other.m_ShaderStages;
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
    m_ShaderStages = other.m_ShaderStages;
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

ShaderReflectionEntryPointsInfo ShaderReflection::GetEntryPointsInfo(const assetlib::ShaderHeader& shader)
{
    ShaderReflectionEntryPointsInfo info = {};
    info.Count = (u32)shader.EntryPoints.size();
    for (u32 i = 0; i < shader.EntryPoints.size(); i++)
    {
        info.Stages[i] = shaderStageFromAssetShaderStage(shader.EntryPoints[i].ShaderStage);
        info.Names[i] = shader.EntryPoints[i].Name;
    }

    return info;
}