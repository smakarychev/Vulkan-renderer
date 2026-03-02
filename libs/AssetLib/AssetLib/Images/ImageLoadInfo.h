#pragma once

#include <AssetLib/Format/ImageFormat.h>
#include <AssetLib/Io/AssetIo.h>

namespace lux::assetlib
{
struct ImageLoadInfo
{
    std::string ImagePath{};
    bool PregeneratedMipmaps{true};
    bool RuntimeMipmaps{true};
    bool IsCubemap{false};
    ImageFormat BakedFormat{ImageFormat::RGBA8_SRGB};
};

namespace image
{
io::IoResult<ImageLoadInfo> readLoadInfo(const std::filesystem::path& path);
io::IoResult<std::string> packLoadInfo(const ImageLoadInfo& imageLoadInfo);
}
}
