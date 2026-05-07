#pragma once

#include <AssetLib/Scenes/GeometryBuffer/GeometryBufferAsset.h>
#include <AssetLib/Scenes/GeometryBuffer/GeometryBufferMeta.h>
#include <AssetImportLib/Importers/Importer.h>

namespace lux::import
{
struct GeometryBufferImportedAsset : ImportedAsset
{
    assetlib::GeometryBufferAsset Asset{};
};

class GeometryBufferImporter final : public Importer
{
public:
    struct MetadataHint
    {
        bool IsSubAsset{false};
        u32 SourceHash{};
        std::string SourceUri{};
    };
    
    GeometryBufferImporter(const std::shared_ptr<Context>& ctx, const MetadataHint& metadataHint);

    using Importer::Import;
    ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) override;
    
    ImportResult<assetlib::AssetId> Export(const assetlib::GeometryBufferAsset& asset, 
        const std::filesystem::path& exportPath);
    
    const ImportedAsset& GetImportedAsset() const override { return GetImportedBuffer(); }
    const GeometryBufferImportedAsset& GetImportedBuffer() const { return m_ImportedAsset; }
    
    const assetlib::AssetMetadata& GetImportedAssetMetadata() const override { return m_ImportedMeta.Metadata; }
    const assetlib::GeometryBufferMeta& GetImportedBufferMetadata() const { return m_ImportedMeta; }
    
    bool NeedsBaking(const std::filesystem::path& path) const override;
    std::filesystem::path GetMetaPath(const std::filesystem::path& path) const override;
protected:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath) override;
private:
    GeometryBufferImportedAsset m_ImportedAsset{};
    assetlib::GeometryBufferMeta m_ImportedMeta{};
    MetadataHint m_MetadataHint{};
};
}
