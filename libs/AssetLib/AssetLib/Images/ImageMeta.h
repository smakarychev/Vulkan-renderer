#pragma once

#include <AssetLib/Format/ImageFormat.h>
#include <AssetLib/Io/AssetIo.h>

namespace lux::assetlib
{
struct ImageMeta
{
    AssetMetadata Metadata{};
    bool PregeneratedMipmaps{true};
    bool RuntimeMipmaps{true};
    bool IsCubemap{false};
    ImageFormat BakedFormat{ImageFormat::RGBA8_SRGB};
    std::optional<f32> HdrExposure{std::nullopt};
};

namespace image
{
io::IoResult<ImageMeta> readMeta(const std::filesystem::path& path);
io::IoResult<std::string> packMeta(const ImageMeta& imageMeta);

static constexpr AssetType ASSET_TYPE = "66c40884-12d6-4f3f-a801-34d585c692a3"_guid;
AssetTypeMetadata getTypeMetadata();
}
}
