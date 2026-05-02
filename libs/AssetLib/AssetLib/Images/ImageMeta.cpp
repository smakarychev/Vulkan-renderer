#include "ImageMeta.h"

#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <> struct glz::meta<lux::assetlib::ImageMeta> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
io::IoResult<ImageMeta> image::readMeta(const std::filesystem::path& path)
{
    DEFINE_BASIC_METADATA_READ(ImageMeta, meta, path)
    return meta;
}

io::IoResult<std::string> image::packMeta(const ImageMeta& imageMeta)
{
    DEFINE_BASIC_METADATA_PACK(ImageMeta, meta, imageMeta)
    return *meta;
}

AssetTypeMetadata image::getTypeMetadata()
{
    static constexpr u32 IMAGE_ASSET_VERSION = 1;

    return {
        .Type = ASSET_TYPE,
        .Name = "image",
        .Version = IMAGE_ASSET_VERSION,
    };
}
}
