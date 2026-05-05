#pragma once

#include <AssetLib/Images/ImageAsset.h>
#include <AssetLib/Images/ImageMeta.h>
#include <AssetImportLib/Bakers/Images/ImageBaker.h>
#include <AssetImportLib/Importers/Importer.h>

namespace lux::import
{
struct ImageImportedAsset : ImportedAsset
{
    assetlib::ImageAsset Asset{};
};

class ImageImporter final : public Importer
{
public:
    struct MetadataHint
    {
        enum class NameHeuristics : u8
        {
            None, Gltf,
        };
        /* Hint for generating `.meta` file for to-be-baked image
        * `BakedFormat` and `NameHeuristics` are used to determine the baked image format
        * If `NameHeuristics` and `BakedFormat` are both not set, the default srgb format will be chosen
        * if `NameHeuristics` is Gltf, the image format will be chosen in accordance with gltf spec.
        * `BakedFormat` takes priority over `NameHeuristics`
        */
        assetlib::ImageFormat BakedFormat{assetlib::ImageFormat::Undefined};
        NameHeuristics Heuristics{NameHeuristics::Gltf};
        bool Overwrite{false};
    };
    
    ImageImporter(const std::shared_ptr<Context>& ctx, const MetadataHint& metadataHint);

    using Importer::Import;
    ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) override;
    
    const ImportedAsset& GetImportedAsset() const override { return GetImportedImage(); }
    const ImageImportedAsset& GetImportedImage() const { return m_ImportedAsset; }
    
    const assetlib::AssetMetadata& GetImportedAssetMetadata() const override { return m_ImportedMeta.Metadata; }
    const assetlib::ImageMeta& GetImportedImageMetadata() const { return m_ImportedMeta; }
    
    bool NeedsBaking(const std::filesystem::path& path) const override;
    std::filesystem::path GetMetaPath(const std::filesystem::path& path) const override;
protected:
    IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath) override;
private:
    ImageImportedAsset m_ImportedAsset{};
    assetlib::ImageMeta m_ImportedMeta{};
    ImageBaker m_Baker;
    MetadataHint m_MetadataHint{};
};
}
