#pragma once

#include <AssetLib/Scenes/Scene/SceneAsset.h>
#include <AssetLib/Scenes/Scene/SceneMeta.h>
#include <AssetImportLib/Importers/Importer.h>
#include <AssetImportLib/Bakers/Scenes/SceneBaker.h>

namespace lux::import
{
struct SceneImportedAsset : ImportedAsset
{
    assetlib::SceneAsset Asset{};
};

class SceneImporter final : public Importer
{
public:
    SceneImporter(const std::shared_ptr<Context>& ctx);

    using Importer::Import;
    ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) override;
    
    const ImportedAsset& GetImportedAsset() const override { return GetImportedScene(); }
    const SceneImportedAsset& GetImportedScene() const { return m_ImportedAsset; }
    
    const assetlib::AssetMetadata& GetImportedAssetMetadata() const override { return m_ImportedMeta.Metadata; }
    const assetlib::SceneMeta& GetImportedSceneMetadata() const { return m_ImportedMeta; }
    
    bool NeedsBaking(const std::filesystem::path& path) const override;
    std::filesystem::path GetMetaPath(const std::filesystem::path& path) const override;

    static std::optional<u32> CalculateGeometryBufferHash(const std::filesystem::path& path, const std::string& uri);
protected:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath) override;
private:
    SceneImportedAsset m_ImportedAsset{};
    assetlib::SceneMeta m_ImportedMeta{};
    SceneBaker m_Baker;
};
}