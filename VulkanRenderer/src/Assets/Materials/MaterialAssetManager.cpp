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
    return false;
}

void MaterialAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (path.extension() != import::MATERIAL_ASSET_EXTENSION)
        return;

    MaterialHandle cached;
    import::MaterialImporter importer(m_Ctx);
    const assetlib::AssetId id = m_AssetSystem->ResolveMetaPath(importer.GetMetaPath(path));
    {
        Lock lock(m_ResourceAccessMutex);
        cached = m_Materials.Find(id);
        if (!cached.IsValid())
            return;
    }
    auto newMaterial = DoLoad(importer, path);
    {
        Lock lock(m_ResourceAccessMutex);
        if (newMaterial.has_value())
            m_Materials[cached.Index()] = std::move(*newMaterial);
    }

    m_AssetSystem->NotifyAssetUpdate(assetlib::material::ASSET_TYPE, {.AssetHandle = cached});
}

MaterialHandle MaterialAssetManager::LoadAsset(const MaterialLoadParameters& parameters)
{
    const std::filesystem::path path = weakly_canonical(parameters.Path).generic_string();
    import::MaterialImporter importer(m_Ctx);
    const assetlib::AssetId id = m_AssetSystem->ResolveMetaPath(importer.GetMetaPath(path));

    const MaterialHandle cached = m_Materials.Find(id);
    if (cached.IsValid())
        return cached;

    auto material = DoLoad(importer, path);
    if (!material.has_value())
        return {};

    return m_Materials.Add(std::move(*material), id);
}

void MaterialAssetManager::UnloadAsset(MaterialHandle handle)
{
    const assetlib::AssetId id = m_Materials.Find(handle);
    if (!id.HasValue())
        return;

    const auto* assetInfo = m_AssetSystem->Resolve(id);
    if (!assetInfo)
        return;

    LUX_LOG_INFO("Unloading material: {}", assetInfo->Path.string());

    m_Materials.Erase(handle, id);
}

const MaterialAsset* MaterialAssetManager::GetAsset(MaterialHandle handle) const
{
    return &m_Materials[handle.Index()];
}

std::optional<MaterialAsset> MaterialAssetManager::DoLoad(import::MaterialImporter& importer,
    const std::filesystem::path& path) const
{
    LUX_LOG_INFO("Loading material: {}", path.string());

    auto imported = importer.Import(path);
    if (!imported.has_value())
    {
        LUX_LOG_ERROR("Failed to load material: {} ({})", imported.error(), path.string());
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
