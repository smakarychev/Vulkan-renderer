#include "rendererpch.h"
#include "ShaderAssetManager.h"

#include "Assets/AssetSystem.h"
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

    m_TextureHeapLayout = Device::CreateDescriptorsLayout({
        .Bindings = {GetTextureHeapBinding()},
        .Flags = descriptors::BINDLESS_DESCRIPTORS_LAYOUT_FLAGS
    });
}

void ShaderAssetManager::Shutdown()
{
    const std::scoped_lock lock(m_ResourceAccessMutex);
    m_ShaderPipelines.clear();
}

DescriptorsLayout ShaderAssetManager::GetTextureHeapLayout() const
{
    const std::scoped_lock lock(m_ResourceAccessMutex);
    
    return m_TextureHeapLayout;
}

DescriptorBinding ShaderAssetManager::GetTextureHeapBinding() const
{
    static const DescriptorBinding descriptorBinding = {
        .Binding = BINDLESS_DESCRIPTORS_TEXTURE_BINDING_INDEX,
        .Type = DescriptorType::Image,
        .Count = descriptors::safeBindlessCountForDescriptorType(DescriptorType::Image),
        .Shaders = ShaderStage::Vertex | ShaderStage::Pixel | ShaderStage::Compute,
        .Flags = descriptors::BINDLESS_DESCRIPTORS_FLAGS,
    };
    
    return descriptorBinding;
}

ShaderHandle ShaderAssetManager::LoadAsset(const ShaderLoadParameters& parameters)
{
    auto defines = GetDefinesFromParameters(parameters);
    const import::ShaderImportSettings settings = CreateBakeSettings(parameters, defines);
    import::ShaderImporter importer(m_Ctx, settings);
    
    const AssetPath assetPath = m_ShaderNameToAssetPath[parameters.Name];
    std::optional<ShaderAsset> shader = DoLoad(importer, assetPath);
    if (!shader.has_value())
        return {};

    return m_ShaderPipelines.Insert(std::move(*shader));
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

    auto shader = DoLoad(importer, assetPath);
    if (!shader.has_value())
        return;
    
    {
        Lock lock(m_ResourceAccessMutex);
        m_ShaderPipelines[handle] = std::move(*shader);
    }
    
    AssetSystem::Get().NotifyAssetUpdate({.Type = assetlib::shader::ASSET_TYPE, .AssetHandle = handle});
}

void ShaderAssetManager::UnloadAsset(ShaderHandle)
{
    ASSERT(false, "Unload is not supported for shader assets")
}

const ShaderAsset* ShaderAssetManager::GetAsset(ShaderHandle handle) const
{
    const auto& shader = m_ShaderPipelines[handle];

    return &shader;
}

std::optional<ShaderAsset> ShaderAssetManager::DoLoad(import::ShaderImporter& importer,
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
    auto& loadInfo = importer.GetImportedShaderLoadInfo();
    
    auto shaderReflectionResult = ShaderReflection::Reflect(shaderAsset, loadInfo);
    if (!shaderReflectionResult.has_value())
        return std::nullopt;

    ShaderReflection::EntryPointsInfo entryPointsInfo = shaderReflectionResult->GetEntryPointsInfo();
    std::array<DescriptorsLayout, MAX_DESCRIPTOR_SETS> descriptorLayoutOverrides{};
    descriptorLayoutOverrides[BINDLESS_DESCRIPTORS_INDEX] = m_TextureHeapLayout;

    ShaderPipelineTemplate pipelineTemplate(ShaderPipelineTemplateCreateInfo{
        .ShaderReflection = std::move(*shaderReflectionResult),
        .DescriptorLayoutOverrides = descriptorLayoutOverrides
    });
    const bool hasTextureHeap = pipelineTemplate.GetSetPresence()[BINDLESS_DESCRIPTORS_INDEX];
    
    ShaderAsset asset = {
        .PipelineTemplate = std::move(pipelineTemplate),
        .HasTextureHeap = hasTextureHeap
    };

    return asset;
}

std::vector<std::pair<std::string, std::string>> ShaderAssetManager::GetDefinesFromParameters(
    const ShaderLoadParameters& parameters) const
{
    auto& overrides = parameters.Defines;
    std::vector<std::pair<std::string, std::string>> defines;
    defines.reserve(overrides.Defines.size());
    for (auto& define : overrides.Defines)
        defines.emplace_back(define.Name.AsStringView(), define.Value);
    
    return defines;
}

import::ShaderImportSettings ShaderAssetManager::CreateBakeSettings(const ShaderLoadParameters& parameters,
    std::vector<std::pair<std::string, std::string>>& defines) const
{
    return {
        .Defines = defines,
        .DefinesHash = parameters.Defines.Hash,
        .Variant = parameters.Variant,
        .IncludePaths = m_BakeSettings->IncludePaths,
        .EnableHotReloading = (bool)CVars::Get().GetI32CVar("Renderer.Shaders.HotReload"_hsv).value_or(true)
    };
}
}
