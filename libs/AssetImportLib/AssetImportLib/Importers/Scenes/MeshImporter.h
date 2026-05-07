#pragma once

#include <AssetLib/Scenes/Mesh/MeshAsset.h>
#include <AssetLib/Scenes/Mesh/MeshMeta.h>
#include <AssetImportLib/Importers/Importer.h>

namespace lux::import
{
struct MeshImportedAsset : ImportedAsset
{
    assetlib::MeshAsset Asset{};
};

class MeshImporter final : public Importer
{
public:
    MeshImporter(const std::shared_ptr<Context>& ctx);

    using Importer::Import;
    ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) override;
    
    ImportResult<assetlib::AssetId> Export(const assetlib::MeshAsset& asset, 
        const std::filesystem::path& exportPath);
    
    const ImportedAsset& GetImportedAsset() const override { return GetImportedMesh(); }
    const MeshImportedAsset& GetImportedMesh() const { return m_ImportedAsset; }
    
    const assetlib::AssetMetadata& GetImportedAssetMetadata() const override { return m_ImportedMeta.Metadata; }
    const assetlib::MeshMeta& GetImportedMeshMetadata() const { return m_ImportedMeta; }
    
    bool NeedsBaking(const std::filesystem::path& path) const override { return false; }
    std::filesystem::path GetMetaPath(const std::filesystem::path& path) const override;
protected:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath) override;
private:
    MeshImportedAsset m_ImportedAsset{};
    assetlib::MeshMeta m_ImportedMeta{};
};
}
