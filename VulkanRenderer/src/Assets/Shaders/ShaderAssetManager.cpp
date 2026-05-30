#include "rendererpch.h"
#include "ShaderAssetManager.h"

#include "FrameContext.h"
#include "Assets/AssetSystem.h"
#include "Assets/Enums/ConvertAssetEnums.h"
#include "cvars/CVarSystem.h"
#include "Vulkan/Device.h"

#include <AssetImportLib/Importers/Import.h>
#include <AssetImportLib/Importers/Shaders/ShaderImporter.h>

namespace lux
{
bool ShaderAssetManager::AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path& metaPath)
{
    if (metadata.Type.Type != assetlib::shader::ASSET_TYPE)
        return false;

    import::ShaderImporter importer(m_Ctx, {});
    auto imported = importer.Import(metaPath, import::ImportFlags::Header);
    if (!imported)
        return false;

    auto& shaderHeader = importer.GetImportedShader().Asset.Header;

    auto shaderLoadInfo = assetlib::shader::readLoadInfo(metadata.Io.OriginalFile);
    if (!shaderLoadInfo.has_value())
        return false;

    const StringId name = StringId::FromString(shaderLoadInfo->Name);
    m_ShaderNameToRawPath.emplace(name, std::filesystem::weakly_canonical(metadata.Io.OriginalFile).generic_string());

    for (auto& include : shaderHeader.Includes)
        if (std::ranges::find(m_RawPathToShaders[include], name) == m_RawPathToShaders[include].end())
            m_RawPathToShaders[include].push_back(name);

    return true;
}

bool ShaderAssetManager::Imports(std::string_view extension)
{
    return
        extension == import::SHADER_ASSET_LOAD_EXTENSION ||
        extension == import::SHADER_ASSET_RAW_EXTENSION;
}

void ShaderAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (Imports(path.extension().string()) || Imports(assetlib::getMetadataRawExtension(path)))
        return OnRawFileModified(path);
}

void ShaderAssetManager::Init(const import::ShaderImportSettings& bakeSettings)
{
    m_BakeSettings = &bakeSettings;
}

void ShaderAssetManager::Shutdown()
{
    for (const PipelineInfo& pipeline : m_Pipelines |
         std::views::filter([](const PipelineInfo& pipeline) { return pipeline.Pipeline.HasValue(); }))
        Device::Destroy(pipeline.Pipeline);

    m_ShaderPipelineTemplates.clear();
}

void ShaderAssetManager::OnFrameBegin(FrameContext& ctx)
{
    m_FrameDeletionQueue = &ctx.DeletionQueue;
}

ShaderCacheAllocateResult ShaderAssetManager::Allocate(ShaderHandle handle,
    DescriptorArenaAllocators& allocators)
{
    auto& pipelineInfo = m_Pipelines[handle.Index()];

    const auto setPresence = pipelineInfo.PipelineTemplate->GetSetPresence();
    for (u32 i = 0; i < MAX_DESCRIPTOR_SETS; i++)
    {
        if (!setPresence[i])
            continue;

        const auto& setInfo = pipelineInfo.PipelineTemplate->GetReflection().DescriptorSetsInfo()[i];
        const bool setIsBindless = setInfo.HasBindless;
        const bool setIsTextureHeap = setIsBindless &&
            setInfo.Descriptors.size() == 1 &&
            setInfo.Descriptors.front().Type == DescriptorType::Image;
        ASSERT(!setIsBindless || setIsTextureHeap, "Shader has bindless set that is not a texture heap")

        if (setIsBindless)
            continue;

        const DescriptorsLayout descriptorsLayout = pipelineInfo.PipelineTemplate->GetDescriptorsLayout(i);

        std::optional<Descriptors> descriptors = Device::AllocateDescriptors(
            allocators.GetTransient(i),
            descriptorsLayout, {
                .Bindings = pipelineInfo.PipelineTemplate->GetReflection().DescriptorSetsInfo()[i].Descriptors,
                .BindlessCount = 0
            });
        if (!descriptors.has_value())
            return std::unexpected(ShaderAssetManagerError::FailedToAllocateDescriptors);

        pipelineInfo.Descriptors[i] = *descriptors;
        pipelineInfo.DescriptorLayouts[i] = descriptorsLayout;
    }

    if (pipelineInfo.HasTextureHeap)
    {
        pipelineInfo.Descriptors[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeap.Descriptors;
        pipelineInfo.DescriptorLayouts[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeap.Layout;
    }

    return {};
}

ShaderCacheTextureHeapResult ShaderAssetManager::AllocateTextureHeap(DescriptorArenaAllocator persistentAllocator,
    u32 count)
{
    if (m_TextureHeap.Descriptors.HasValue())
    {
        LUX_LOG_WARN("Texture heap is already added");
        return std::unexpected(ShaderAssetManagerError::FailedToAllocateDescriptors);
    }

    const DescriptorBinding descriptorBinding = {
        .Binding = BINDLESS_DESCRIPTORS_TEXTURE_BINDING_INDEX,
        .Type = DescriptorType::Image,
        .Count = descriptors::safeBindlessCountForDescriptorType(DescriptorType::Image),
        .Shaders = ShaderStage::Vertex | ShaderStage::Pixel | ShaderStage::Compute,
        .Flags = descriptors::BINDLESS_DESCRIPTORS_FLAGS,
    };
    const DescriptorsLayout layout = Device::CreateDescriptorsLayout({
        .Bindings = {descriptorBinding},
        .Flags = descriptors::BINDLESS_DESCRIPTORS_LAYOUT_FLAGS
    });

    const auto allocation = Device::AllocateDescriptors(persistentAllocator, layout, {
        .Bindings = {descriptorBinding},
        .BindlessCount = count,
    });

    if (!allocation.has_value())
        return std::unexpected(ShaderAssetManagerError::FailedToAllocateDescriptors);

    m_TextureHeap = {
        .Descriptors = *allocation,
        .Layout = layout
    };

    std::array<DescriptorsLayout, BINDLESS_DESCRIPTORS_INDEX + 1> descriptorSets;
    std::ranges::fill(descriptorSets, Device::GetEmptyDescriptorsLayout());
    descriptorSets[BINDLESS_DESCRIPTORS_INDEX] = layout;

    const PipelineLayout pipelineLayout = Device::CreatePipelineLayout({
        .DescriptorsLayouts = descriptorSets
    });

    return ShaderTextureHeapAllocation{
        .Descriptors = *allocation,
        .PipelineLayout = pipelineLayout
    };
}

ShaderHandle ShaderAssetManager::LoadAsset(const ShaderLoadParameters& parameters)
{
    const ShaderNameWithOverrides nameWithOverrides = {
        .Name = parameters.Name,
        .Variant = parameters.Variant.value_or(import::ShaderBaker::MAIN_VARIANT),
        .OverridesHash = parameters.Overrides->Hash
    };
    RebakeInfo rebakeInfo = CreateRebakeInfo(nameWithOverrides, parameters);
    const import::ShaderImportSettings settings = CreateBakeSettings(rebakeInfo);
    import::ShaderImporter importer(m_Ctx, settings);
    
    auto it = m_PipelinesMap.find(nameWithOverrides);
    if (it != m_PipelinesMap.end())
    {
        if (m_Pipelines[it->second.Index()].ShouldReload)
            ReloadPipeline(m_Pipelines[it->second.Index()], importer, parameters);
        
        return it->second;
    }
    
    const std::filesystem::path rawPath = m_ShaderNameToRawPath[parameters.Name];
    std::optional<LoadedPipelineInfo> loadedPipeline = DoLoad(importer, rawPath);
    if (!loadedPipeline.has_value())
        return {};

    const PipelineInfo newPipeline = {
        .PipelineTemplate = loadedPipeline->PipelineTemplate,
        .Pipeline = CreatePipeline(importer.GetImportedShaderLoadInfo(), *loadedPipeline->PipelineTemplate, parameters),
        .Layout = loadedPipeline->Layout,
        .HasTextureHeap = loadedPipeline->HasTextureHeap,
    };
    
    auto& pathRebakes = m_RawPathToRebakeInfos[rawPath.string()];
    if (std::ranges::find(pathRebakes, rebakeInfo) == pathRebakes.end())
        pathRebakes.push_back(std::move(rebakeInfo));

    ShaderHandle handle;
    if (it == m_PipelinesMap.end())
    {
        handle = ShaderHandle((u32)m_Pipelines.size(), 0);
        m_Pipelines.push_back(newPipeline);
        m_PipelinesMap[nameWithOverrides] = handle;
    }
    else
    {
        handle = it->second;
        m_FrameDeletionQueue->Enqueue(m_Pipelines[handle.Index()].Pipeline);
        m_Pipelines[handle.Index()] = newPipeline;
    }

    return handle;
}

void ShaderAssetManager::UnloadAsset(ShaderHandle)
{
    ASSERT(false, "Unload is not supported for shader assets")
}

ShaderAssetManager::GetType ShaderAssetManager::GetAsset(ShaderHandle handle) const
{
    const auto& pipelineInfo = m_Pipelines[handle.Index()];

    ShaderAsset shader = {};
    shader.m_Pipeline = pipelineInfo.Pipeline;
    shader.m_PipelineLayout = pipelineInfo.Layout;
    shader.m_Descriptors = pipelineInfo.Descriptors;
    shader.m_DescriptorLayouts = pipelineInfo.DescriptorLayouts;

    return shader;
}

void ShaderAssetManager::OnRawFileModified(const std::filesystem::path& path)
{
    Lock lock(m_ResourceAccessMutex);

    for (const StringId name : m_RawPathToShaders[path.generic_string()])
    {
        const std::filesystem::path& shaderPath = m_ShaderNameToRawPath[name];
        if (!m_RawPathToRebakeInfos.contains(shaderPath.string()))
            continue;

        for (auto& rebakeInfo : m_RawPathToRebakeInfos[shaderPath.string()])
        {
            m_AssetSystem->AddImportRequest({
                .ImportFn = [this, shaderPath, &rebakeInfo]()
                {
                    const import::ShaderImportSettings settings = CreateBakeSettings(rebakeInfo);
                    import::ShaderImporter importer(m_Ctx, settings);
                    
                    auto pipelineInfo = DoLoad(importer, shaderPath);
                    if (!pipelineInfo.has_value())
                        return;
                    {
                        Lock lock(m_ResourceAccessMutex);
                        auto pipelineIt = m_PipelinesMap.find(rebakeInfo.NameWithOverrides);
                        if (pipelineIt == m_PipelinesMap.end())
                            return;
                        
                        auto& existingPipeline = m_Pipelines[pipelineIt->second.Index()];
                        existingPipeline.PipelineTemplate = pipelineInfo->PipelineTemplate;
                        existingPipeline.HasTextureHeap = pipelineInfo->HasTextureHeap;
                        existingPipeline.Layout = pipelineInfo->Layout;
                        existingPipeline.ShouldReload = true;
                    }
                }
            });
        }
    }
}

std::optional<ShaderAssetManager::LoadedPipelineInfo> ShaderAssetManager::DoLoad(import::ShaderImporter& importer,
    const std::filesystem::path& path)
{
    LUX_LOG_INFO("Loading shader: {}", path.string());
    
    auto imported = importer.Import(path);
    if (!imported.has_value())
    {
        LUX_LOG_ERROR("Failed to load shader: {} ({})", imported.error(), path.string());
        return std::nullopt;
    }
    
    auto& shaderAsset = importer.GetImportedShader().Asset;
    auto& shaderMetadata = importer.GetImportedAssetMetadata();
    
    auto shaderReflectionResult = ShaderReflection::Reflect(shaderAsset);
    if (!shaderReflectionResult.has_value())
        return std::nullopt;

    ShaderReflection::EntryPointsInfo entryPointsInfo = shaderReflectionResult->GetEntryPointsInfo();
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> descriptorLayoutOverrides{};
    descriptorLayoutOverrides[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeap.Layout;

    const ShaderPipelineTemplate* pipelineTemplate = &(m_ShaderPipelineTemplates[shaderMetadata.AssetId] =
        ShaderPipelineTemplate({
            .ShaderReflection = std::move(*shaderReflectionResult),
            .DescriptorLayoutOverrides = descriptorLayoutOverrides
        })
    );
    
    LoadedPipelineInfo pipelineInfo = {};
    pipelineInfo.PipelineTemplate = pipelineTemplate;
    pipelineInfo.Layout = pipelineTemplate->GetPipelineLayout();
    pipelineInfo.HasTextureHeap = pipelineTemplate->GetSetPresence()[BINDLESS_DESCRIPTORS_INDEX];

    return pipelineInfo;
}

Pipeline ShaderAssetManager::CreatePipeline(const assetlib::ShaderLoadInfo& shaderLoadInfo,
    const ShaderPipelineTemplate& pipelineTemplate, const ShaderLoadParameters& parameters)
{
    std::vector<Format> colorFormats;
    std::optional<Format> depthFormat;
    DynamicStates dynamicStates = DynamicStates::Default;
    AlphaBlending alphaBlending = AlphaBlending::Over;
    DepthMode depthMode = DepthMode::ReadWrite;
    DepthTest depthTest = DepthTest::GreaterOrEqual;
    FaceCullMode cullMode = FaceCullMode::Back;
    PrimitiveKind primitiveKind = PrimitiveKind::Triangle;
    bool clampDepth = false;

    if (shaderLoadInfo.RasterizationInfo.has_value())
    {
        const auto& rasterization = *shaderLoadInfo.RasterizationInfo;
        dynamicStates = dynamicStatesFromAssetDynamicStates(rasterization.DynamicStates);
        alphaBlending = alphaBlendingFromAssetAlphaBlending(rasterization.AlphaBlending);
        depthMode = depthModeFromAssetDepthMode(rasterization.DepthMode);
        depthTest = depthTestFromAssetDepthTest(rasterization.DepthTest);
        cullMode = faceCullModeFromAssetFaceCullMode(rasterization.FaceCullMode);
        primitiveKind = primitiveKindModeFromAssetPrimitiveKind(rasterization.PrimitiveKind);
        clampDepth = rasterization.ClampDepth;

        colorFormats.reserve(rasterization.Colors.size());
        for (auto& color : rasterization.Colors)
            colorFormats.push_back(formatFromAssetImageFormat(color.Format));
        if (rasterization.Depth.has_value())
            depthFormat = formatFromAssetImageFormat(*rasterization.Depth);
    }

    ASSERT(pipelineTemplate.GetReflection().Shaders().size() == 1)
    std::array<ShaderModule, MAX_PIPELINE_SHADER_COUNT> shaderModules{};
    std::ranges::fill(shaderModules, pipelineTemplate.GetReflection().Shaders().front());

    const auto& overrides = parameters.Overrides;
    const Pipeline pipeline = Device::CreatePipeline({
        .PipelineLayout = pipelineTemplate.GetPipelineLayout(),
        .Shaders = Span((const ShaderModule*)shaderModules.data(), pipelineTemplate.GetShaderStages().size()),
        .ShaderStages = pipelineTemplate.GetShaderStages(),
        .ShaderEntryPoints = pipelineTemplate.GetEntryPoints(),
        .ColorFormats = colorFormats,
        .DepthFormat = depthFormat ? *depthFormat : Format::Undefined,
        .DynamicStates = overrides->PipelineOverrides.DynamicStates.value_or(dynamicStates),
        .DepthMode = overrides->PipelineOverrides.DepthMode.value_or(depthMode),
        .DepthTest = overrides->PipelineOverrides.DepthTest.value_or(depthTest),
        .CullMode = overrides->PipelineOverrides.CullMode.value_or(cullMode),
        .AlphaBlending = overrides->PipelineOverrides.AlphaBlending.value_or(alphaBlending),
        .PrimitiveKind = overrides->PipelineOverrides.PrimitiveKind.value_or(primitiveKind),
        .Specialization = overrides->Specializations.ToPipelineSpecializationsView(pipelineTemplate),
        .IsComputePipeline = pipelineTemplate.IsComputeTemplate(),
        .ClampDepth = overrides->PipelineOverrides.ClampDepth.value_or(clampDepth)
    }, Device::DummyDeletionQueue());
    Device::NamePipeline(pipeline, parameters.Name.AsStringView());

    return pipeline;
}

void ShaderAssetManager::ReloadPipeline(PipelineInfo& pipelineInfo, import::ShaderImporter& importer,
    const ShaderLoadParameters& parameters)
{
    const std::filesystem::path rawPath = m_ShaderNameToRawPath[parameters.Name];
    auto imported = importer.Import(rawPath, import::ImportFlags::Header);
    if (!imported)
        return;
    
    m_FrameDeletionQueue->Enqueue(pipelineInfo.Pipeline);
    pipelineInfo.ShouldReload = false;
    pipelineInfo.Pipeline = CreatePipeline(importer.GetImportedShaderLoadInfo(), *pipelineInfo.PipelineTemplate, 
        parameters);
}

ShaderAssetManager::RebakeInfo ShaderAssetManager::CreateRebakeInfo(const ShaderNameWithOverrides& name, 
    const ShaderLoadParameters& parameters) const
{
    auto& overrides = *parameters.Overrides;
    std::vector<std::pair<std::string, std::string>> defines;
    defines.reserve(overrides.Defines.Defines.size());
    for (auto& define : overrides.Defines.Defines)
        defines.emplace_back(define.Name.AsStringView(), define.Value);
    
    return {
        .NameWithOverrides = name,
        .DefinesHash = overrides.Defines.Hash,
        .Defines = defines,
    };
}

import::ShaderImportSettings ShaderAssetManager::CreateBakeSettings(const RebakeInfo& rebakeInfo) const
{
    return {
        .Defines = rebakeInfo.Defines,
        .DefinesHash = rebakeInfo.DefinesHash,
        .Variant = rebakeInfo.NameWithOverrides.Variant,
        .IncludePaths = m_BakeSettings->IncludePaths,
        .EnableHotReloading = (bool)CVars::Get().GetI32CVar("Renderer.Shaders.HotReload"_hsv).value_or(true)
    };
}
}
