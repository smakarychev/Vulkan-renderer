#pragma once

#include <AssetLib/Images/ImageAsset.h>
#include <AssetImportLib/Importers/ImportContext.h>
#include <AssetImportLib/Importers/Import.h>

namespace lux::assetlib
{
struct ImageMeta;
}

namespace lux::import
{
class ImageBaker
{
public:
    ImageBaker(const std::shared_ptr<Context>& ctx) : m_Ctx(ctx) {}
    
    IoResult<std::filesystem::path> BakeToFile(assetlib::ImageMeta& meta, const std::filesystem::path& metaPath);

    bool NeedsBaking(const std::filesystem::path& metaPath) const;
private:
    IoResult<assetlib::ImageAsset> Bake(const assetlib::ImageMeta& meta);
    IoResult<assetlib::ImageAsset> BakeHDR(const assetlib::ImageMeta& meta);
    IoResult<assetlib::ImageAsset> BakeLDRKtx(const assetlib::ImageMeta& meta);
    IoResult<assetlib::ImageAsset> BakeLDRJpg(const assetlib::ImageMeta& meta);
private:
    std::shared_ptr<Context> m_Ctx{nullptr};
};
}
