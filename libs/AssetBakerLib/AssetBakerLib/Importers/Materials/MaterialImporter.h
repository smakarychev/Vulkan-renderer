#pragma once

#include <AssetLib/Materials/MaterialAsset.h>
#include <AssetLib/Materials/MaterialMeta.h>
#include <AssetBakerLib/Importers/Importer.h>

namespace lux::bakers
{
struct MaterialImportedAsset : ImportedAsset
{
    assetlib::MaterialAsset Asset{};
};

class MaterialImporter final : public Importer
{
public:
    MaterialImporter(const std::shared_ptr<Context>& ctx);

    using Importer::Import;
    ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) override;
    
    ImportResult<assetlib::AssetId> Export(const assetlib::MaterialAsset& asset, 
        const std::filesystem::path& exportPath);
    
    const ImportedAsset& GetImportedAsset() const override { return GetImportedMaterial(); }
    const MaterialImportedAsset& GetImportedMaterial() const { return m_ImportedAsset; }
    
    const assetlib::AssetMetadata& GetImportedAssetMetadata() const override { return m_ImportedMeta.Metadata; }
    const assetlib::MaterialMeta& GetImportedMaterialMetadata() const { return m_ImportedMeta; }
    
    bool NeedsBaking(const std::filesystem::path& path) const override { return false; }
    std::filesystem::path GetMetaPath(const std::filesystem::path& path) const override;
private:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath);
private:
    MaterialImportedAsset m_ImportedAsset{};
    assetlib::MaterialMeta m_ImportedMeta{};
};
}