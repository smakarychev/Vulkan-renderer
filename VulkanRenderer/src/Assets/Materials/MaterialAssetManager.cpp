#include "rendererpch.h"
#include "MaterialAssetManager.h"

#include "Assets/AssetSystem.h"
#include "Assets/Enums/ConvertAssetEnums.h"
#include "Assets/Images/ImageAssetManager.h"

#include <AssetLib/Images/ImageMeta.h>
#include <AssetLib/Materials/MaterialAsset.h>
#include <AssetLib/Materials/MaterialMeta.h>
#include <AssetImportLib/Importers/Import.h>
#include <AssetImportLib/Importers/Materials/MaterialImporter.h>

namespace lux
{
void MaterialAssetManager::OnAssetSystemInit()
{
    m_ImageAssetManager = m_AssetSystem->GetAssetManagerFor<ImageAssetManager>(assetlib::image::ASSET_TYPE);
    ASSERT(m_ImageAssetManager)
}

bool MaterialAssetManager::AddManaged(const assetlib::AssetMetadata& metadata, const std::filesystem::path&)
{
    return metadata.Type.Type == assetlib::material::ASSET_TYPE;
}

bool MaterialAssetManager::Imports(std::string_view extension)
{
    return extension == import::MATERIAL_ASSET_EXTENSION;
}

void MaterialAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (!Imports(path.extension().string()))
        return;
    
    Lock lock(m_ResourceAccessMutex);
    
    const MaterialHandle handle = FindHandleByLoadParametersUnsafe({.Path = path});
    /* new material was created */
    if (!handle.IsValid())
    {
        m_AssetSystem->RegisterAsset(import::MaterialImporter(m_Ctx).GetMetaPath(path));
        return;
    }

    ReloadAsset(handle);
}

MaterialHandle MaterialAssetManager::LoadAsset(const MaterialLoadParameters& parameters)
{
    import::MaterialImporter importer(m_Ctx);

    auto material = DoLoad(importer, parameters.Path);
    if (!material.has_value())
        return {};

    return m_Materials.Insert(std::move(*material));
}

void MaterialAssetManager::ReloadAsset(MaterialHandle handle)
{
    const auto* parameters = FindLoadParametersUnsafe(handle); 
    if (!parameters)
        return;
    
    import::MaterialImporter importer(m_Ctx);
    
    auto newMaterial = DoLoad(importer, parameters->Path);
    if (!newMaterial)
        return;
    
    m_Materials[handle.Index()] = std::move(*newMaterial);
    m_AssetSystem->NotifyAssetUpdate({.Type = assetlib::material::ASSET_TYPE, .AssetHandle = handle});
}

void MaterialAssetManager::UnloadAsset(MaterialHandle handle)
{
    const auto* parameters = FindLoadParametersUnsafe(handle);
    if (!parameters)
        return;
    
    LUX_LOG_INFO("Unloading material: {}", parameters->Path.ToString());
    
    m_Materials.Erase(handle);
}

const MaterialAsset* MaterialAssetManager::GetAsset(MaterialHandle handle) const
{
    return &m_Materials[handle.Index()];
}

std::optional<MaterialAsset> MaterialAssetManager::DoLoad(import::MaterialImporter& importer, 
    const AssetPath& path) const
{
    LUX_LOG_INFO("Loading material: {}", path.ToString());

    auto imported = importer.Import(path);
    if (!imported.has_value())
    {
        LUX_LOG_ERROR("Failed to load material: {} ({})", imported.error(), path.ToString());
        return std::nullopt;
    }
    auto& materialAsset = importer.GetImportedMaterial().Asset;

    ASSERT(m_ImageAssetManager)

    MaterialAsset material = {
        .Name = StringId::FromString(materialAsset.Name),
        .BaseColor = materialAsset.BaseColor,
        .Metallic = materialAsset.Metallic,
        .Roughness = materialAsset.Roughness,
        .EmissiveFactor = materialAsset.EmissiveFactor,
        .AlphaMode = materialAlphaModeFromAssetAlphaMode(materialAsset.AlphaMode),
        .AlphaCutoff = materialAsset.AlphaCutoff,
        .DoubleSided = materialAsset.DoubleSided,
        .OcclusionStrength = materialAsset.OcclusionStrength,
        .BaseColorTexture = LoadTexture(materialAsset.BaseColorTexture),
        .EmissiveTexture = LoadTexture(materialAsset.EmissiveTexture),
        .NormalTexture = LoadTexture(materialAsset.NormalTexture),
        .MetallicRoughnessTexture = LoadTexture(materialAsset.MetallicRoughnessTexture),
        .OcclusionTexture = LoadTexture(materialAsset.OcclusionTexture),
    };

    return material;
}

ImageHandle MaterialAssetManager::LoadTexture(assetlib::AssetId imageAsset) const
{
    if (!imageAsset.HasValue())
        return {};

    const AssetIdResolver::AssetInfo* imageInfo = m_AssetSystem->Resolve(imageAsset);
    // todo: return dummy image instead
    if (imageInfo == nullptr)
        return {};

    return m_ImageAssetManager->LoadResource({.Path = imageInfo->Path});
}
}
