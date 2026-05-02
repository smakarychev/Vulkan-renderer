#pragma once

#include <AssetLib/Scenes/SceneAsset.h>
#include <AssetLib/Scenes/SceneMeta.h>
#include <AssetBakerLib/Importers/Importer.h>
#include <AssetBakerLib/Bakers/Scenes/SceneBaker.h>

namespace lux::bakers
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
private:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath);
private:
    SceneImportedAsset m_ImportedAsset{};
    assetlib::SceneMeta m_ImportedMeta{};
    SceneBaker m_Baker;
};
}