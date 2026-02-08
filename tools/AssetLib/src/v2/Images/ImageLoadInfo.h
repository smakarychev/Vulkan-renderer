#pragma once

#include "v2/Format/ImageFormat.h"
#include "v2/Io/AssetIo.h"

namespace lux::assetlib
{
struct ImageLoadInfo
{
    std::string ImagePath{};
    bool PregeneratedMipmaps{true};
    bool IsCubemap{false};
    ImageFormat BakedFormat{ImageFormat::RGBA8_SRGB};
};

namespace image
{
io::IoResult<ImageLoadInfo> readLoadInfo(const std::filesystem::path& path);
io::IoResult<std::string> packLoadInfo(const ImageLoadInfo& imageLoadInfo);
}
}
