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
    m_ShaderNameToAssetPath.emplace(name, AssetPath{std::filesystem::weakly_canonical(metadata.Io.OriginalFile)});

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

void ShaderAssetManager::OnFileModified(const AssetPath& path)
{
    if (!Imports(path.RawExtension()))
        return;
    
    Lock lock(m_ResourceAccessMutex);

    for (const StringId name : m_RawPathToShaders[path.RawPath().string()])
    {
        IterateBySourceUnsafe(name, [&](ShaderHandle asset)
        {
            AssetSystem::Get().AddImportRequest({.ImportFn = [this, asset]()
                {
                    ReloadAsset(asset);
                }
            });
        });
    }
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
    Lock lock(m_ResourceAccessMutex);
    m_FrameDeletionQueue = ctx.DeletionQueue.get();
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

        std::optional<Descriptors> descriptors = allocators.GetTransient(i).Allocate(
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

    const auto allocation = persistentAllocator.Allocate(layout, {
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
    auto defines = GetDefinesFromParameters(parameters);
    const import::ShaderImportSettings settings = CreateBakeSettings(parameters, defines);
    import::ShaderImporter importer(m_Ctx, settings);
    
    const AssetPath assetPath = m_ShaderNameToAssetPath[parameters.Name];
    std::optional<LoadedPipelineInfo> loadedPipeline = DoLoad(importer, assetPath);
    if (!loadedPipeline.has_value())
        return {};

    const PipelineInfo newPipeline = {
        .PipelineTemplate = loadedPipeline->PipelineTemplate,
        .Pipeline = CreatePipeline(*loadedPipeline->PipelineTemplate, parameters),
        .Layout = loadedPipeline->Layout,
        .HasTextureHeap = loadedPipeline->HasTextureHeap,
    };

    const ShaderHandle handle((u32)m_Pipelines.size(), 0);
    m_Pipelines.push_back(newPipeline);

    return handle;
}

void ShaderAssetManager::ReloadAsset(ShaderHandle handle)
{
    const ShaderLoadParameters* parameters;
    AssetPath assetPath;
    {
        Lock lock(m_ResourceAccessMutex);
        parameters = FindLoadParametersUnsafe(handle);
        if (!parameters)
            return;
        assetPath = m_ShaderNameToAssetPath.at(parameters->Name);
    }

    auto defines = GetDefinesFromParameters(*parameters);
    const import::ShaderImportSettings settings = CreateBakeSettings(*parameters, defines);
    import::ShaderImporter importer(m_Ctx, settings);
    AssetSystem::Get().SkipFileUpdateNotify(importer.GetMetaPath(assetPath.RawPath()));

    auto pipelineInfo = DoLoad(importer, assetPath);
    if (!pipelineInfo.has_value())
        return;
    
    {
        Lock lock(m_ResourceAccessMutex);
        auto& existingPipeline = m_Pipelines[handle.Index()];
        existingPipeline.PipelineTemplate = pipelineInfo->PipelineTemplate;
        existingPipeline.HasTextureHeap = pipelineInfo->HasTextureHeap;
        existingPipeline.Layout = pipelineInfo->Layout;
        ReloadPipeline(existingPipeline, *parameters);
    }
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

std::optional<ShaderAssetManager::LoadedPipelineInfo> ShaderAssetManager::DoLoad(import::ShaderImporter& importer,
    const AssetPath& path)
{
    LUX_LOG_INFO("Loading shader: {}", path.ToString());
    
    auto imported = importer.Import(path.RawPath());
    if (!imported.has_value())
    {
        LUX_LOG_ERROR("Failed to load shader: {} ({})", imported.error(), path.ToString());
        return std::nullopt;
    }
    
    auto& shaderAsset = importer.GetImportedShader().Asset;
    auto& shaderMetadata = importer.GetImportedAssetMetadata();
    auto& loadInfo = importer.GetImportedShaderLoadInfo();
    
    auto shaderReflectionResult = ShaderReflection::Reflect(shaderAsset, loadInfo);
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

Pipeline ShaderAssetManager::CreatePipeline(const ShaderPipelineTemplate& pipelineTemplate,
    const ShaderLoadParameters& parameters)
{
    auto& reflection = pipelineTemplate.GetReflection();
    PipelineStateSettings stateSettings = reflection.PipelineSettings();

    ASSERT(pipelineTemplate.GetReflection().Shaders().size() == 1)
    std::array<ShaderModule, MAX_PIPELINE_SHADER_COUNT> shaderModules{};
    std::ranges::fill(shaderModules, pipelineTemplate.GetReflection().Shaders().front());

    const auto& overrides = parameters.Overrides;
    const auto& pipelineOverrides = overrides.PipelineOverrides;
    if (pipelineOverrides.DynamicStates)
        stateSettings.DynamicStates = *pipelineOverrides.DynamicStates;
    if (pipelineOverrides.DepthMode)
        stateSettings.DepthMode = *pipelineOverrides.DepthMode;
    if (pipelineOverrides.DepthTest)
        stateSettings.DepthTest = *pipelineOverrides.DepthTest;
    if (pipelineOverrides.CullMode)
        stateSettings.CullMode = *pipelineOverrides.CullMode;
    if (pipelineOverrides.AlphaBlending)
        stateSettings.AlphaBlending = *pipelineOverrides.AlphaBlending;
    if (pipelineOverrides.PrimitiveKind)
        stateSettings.PrimitiveKind = *pipelineOverrides.PrimitiveKind;
    if (pipelineOverrides.ClampDepth)
        stateSettings.ClampDepth = *pipelineOverrides.ClampDepth;
    
    ShaderDynamicSpecializations specializations = overrides.Specializations;
    const Pipeline pipeline = Device::CreatePipeline({
        .PipelineLayout = pipelineTemplate.GetPipelineLayout(),
        .Shaders = Span((const ShaderModule*)shaderModules.data(), pipelineTemplate.GetShaderStages().size()),
        .ShaderStages = pipelineTemplate.GetShaderStages(),
        .ShaderEntryPoints = pipelineTemplate.GetEntryPoints(),
        .ColorFormats = reflection.ColorFormats(),
        .DepthFormat = reflection.DepthFormat() ? *reflection.DepthFormat() : Format::Undefined,
        .StateSettings = stateSettings,
        .Specialization = specializations.ToPipelineSpecializationsView(pipelineTemplate),
        .IsComputePipeline = pipelineTemplate.IsComputeTemplate(),
    }, Device::DummyDeletionQueue());
    Device::NamePipeline(pipeline, parameters.Name.AsStringView());

    return pipeline;
}

void ShaderAssetManager::ReloadPipeline(PipelineInfo& pipelineInfo, const ShaderLoadParameters& parameters)
{
    m_FrameDeletionQueue->Enqueue(pipelineInfo.Pipeline);
    pipelineInfo.Pipeline = CreatePipeline(*pipelineInfo.PipelineTemplate, parameters);
}

std::vector<std::pair<std::string, std::string>> ShaderAssetManager::GetDefinesFromParameters(
    const ShaderLoadParameters& parameters) const
{
    auto& overrides = parameters.Overrides;
    std::vector<std::pair<std::string, std::string>> defines;
    defines.reserve(overrides.Defines.Defines.size());
    for (auto& define : overrides.Defines.Defines)
        defines.emplace_back(define.Name.AsStringView(), define.Value);
    
    return defines;
}

import::ShaderImportSettings ShaderAssetManager::CreateBakeSettings(const ShaderLoadParameters& parameters,
    std::vector<std::pair<std::string, std::string>>& defines) const
{
    return {
        .Defines = defines,
        .DefinesHash = parameters.Overrides.Defines.Hash,
        .Variant = parameters.Variant,
        .IncludePaths = m_BakeSettings->IncludePaths,
        .EnableHotReloading = (bool)CVars::Get().GetI32CVar("Renderer.Shaders.HotReload"_hsv).value_or(true)
    };
}
}
