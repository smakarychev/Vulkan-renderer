#include "rendererpch.h"
#include "MaterialAssetManager.h"

#include "Assets/AssetSystem.h"
#include "Assets/Enums/ConvertAssetEnums.h"
#include "Assets/Images/ImageAssetManager.h"
#include "v2/Materials/MaterialAsset.h"
#include "v2/Images/ImageAsset.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"

namespace lux
{
namespace 
{
constexpr std::string_view MATERIAL_ASSET_EXTENSION = ".mat";
}

bool MaterialAssetManager::AddManaged(const std::filesystem::path& path, AssetIdResolver& resolver)
{
    if (path.extension() != MATERIAL_ASSET_EXTENSION)
        return false;

    auto assetFile = m_AssetSystem->GetIo().ReadHeader(path);
    if (!assetFile.has_value())
        return false;

    auto material = assetlib::material::readMaterial(*assetFile);
    if (!material.has_value())
        return false;

    resolver.RegisterId(assetFile->Metadata.AssetId, {
        .Path = path,
        .AssetType = assetFile->Metadata.Type
    });

    return true;
}

bool MaterialAssetManager::Bakes(const std::filesystem::path& path)
{
    return false;
}

void MaterialAssetManager::OnFileModified(const std::filesystem::path& path)
{
    if (path.extension() != MATERIAL_ASSET_EXTENSION)
        return;

    Lock lock(m_ResourceAccessMutex);

    const MaterialHandle cached = m_Materials.Find(weakly_canonical(path).generic_string());
    if (!cached.IsValid())
        return;

    auto newMaterial = DoLoad({.Path = path});
    if (newMaterial.has_value())
        m_Materials[cached.Index()] = std::move(*newMaterial);
}

MaterialHandle MaterialAssetManager::LoadAsset(const MaterialLoadParameters& parameters)
{
    const std::filesystem::path path = weakly_canonical(parameters.Path).generic_string();
    const MaterialHandle cached = m_Materials.Find(path);
    if (cached.IsValid())
        return cached;

    auto material = DoLoad(parameters);
    if (!material.has_value())
        return {};

    return m_Materials.Add(std::move(*material), path);
}

void MaterialAssetManager::UnloadAsset(MaterialHandle handle)
{
    const std::filesystem::path* path = m_Materials.Find(handle);
    if (path == nullptr)
        return;

    LUX_LOG_INFO("Unloading material: {}", path->string());

    m_Materials.Erase(handle, *path);
}

const MaterialAsset* MaterialAssetManager::GetAsset(MaterialHandle handle) const
{
    return &m_Materials[handle.Index()];
}

std::optional<MaterialAsset> MaterialAssetManager::DoLoad(const MaterialLoadParameters& parameters) const
{
    LUX_LOG_INFO("Loading material: {}", parameters.Path.string());
    
    const auto assetFile = m_AssetSystem->GetIo().ReadHeader(parameters.Path);
    if (!assetFile.has_value())
        return std::nullopt;

    auto materialAsset = assetlib::material::readMaterial(*assetFile);
    if (!materialAsset.has_value())
        return std::nullopt;

    ImageAssetManager* imageAssetManager =
        m_AssetSystem->GetAssetManagerFor<ImageAssetManager>(assetlib::image::getMetadata().Type);
    ASSERT(imageAssetManager)

    MaterialAsset material = {
        .Name = StringId::FromString(materialAsset->Name),
        .BaseColor = materialAsset->BaseColor,
        .Metallic = materialAsset->Metallic,
        .Roughness = materialAsset->Roughness,
        .EmissiveFactor = materialAsset->EmissiveFactor,
        .AlphaMode = materialAlphaModeFromAssetAlphaMode(materialAsset->AlphaMode),
        .AlphaCutoff = materialAsset->AlphaCutoff,
        .DoubleSided = materialAsset->DoubleSided,
        .OcclusionStrength = materialAsset->OcclusionStrength,
        .BaseColorTexture = LoadTexture(imageAssetManager, materialAsset->BaseColorTexture),
        .EmissiveTexture = LoadTexture(imageAssetManager, materialAsset->EmissiveTexture),
        .NormalTexture = LoadTexture(imageAssetManager, materialAsset->NormalTexture),
        .MetallicRoughnessTexture = LoadTexture(imageAssetManager, materialAsset->MetallicRoughnessTexture),
        .OcclusionTexture = LoadTexture(imageAssetManager, materialAsset->OcclusionTexture),
    };

    return material;
}

ImageHandle MaterialAssetManager::LoadTexture(ImageAssetManager* imageAssetManager, assetlib::AssetId imageAsset) const
{
    if (!imageAsset.HasValue())
        return {};
    
    const AssetIdResolver::AssetInfo* imageInfo = m_AssetSystem->Resolve(imageAsset);
    // todo: return dummy image instead
    if (imageInfo == nullptr)
        return {};

    return imageAssetManager->LoadResource({.Path = imageInfo->Path});
}
}
