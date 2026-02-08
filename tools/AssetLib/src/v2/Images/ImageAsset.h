#pragma once

#include "v2/Format/ImageFormat.h"
#include "v2/Io/AssetIo.h"

namespace lux::assetlib
{
namespace io
{
class AssetCompressor;
class AssetIoInterface;
}

enum class ImageKind : u8
{
    Image2d,
    Image3d,
    ImageCubemap,
    Image2dArray,
};
enum class ImageFilter : u8
{
    Linear, Nearest
};
struct ImageMipmapSizes
{
    static constexpr u32 MAX_MIPMAPS = 32;
    std::array<u32, MAX_MIPMAPS> Sizes{};
};
struct ImageHeader
{
    ImageFormat Format{ImageFormat::Undefined};
    ImageKind Kind{ImageKind::Image2d};
    ImageFilter Filter{ImageFilter::Linear};
    u32 Width{0};
    u32 Height{0};
    u32 Depth{0};
    u32 Layers{0};
    u32 Mipmaps{0};
    std::vector<ImageMipmapSizes> LayerSizes{};
};

struct ImageAsset
{
    struct LayerImageData
    {
        std::vector<std::vector<std::byte>> MipmapImageData;
    };
    ImageHeader Header{};
    std::vector<LayerImageData> Layers{};
};

namespace image
{
io::IoResult<ImageHeader> readHeader(const AssetFile& assetFile);
io::IoResult<std::vector<std::byte>> readImageData(const ImageHeader& header, const AssetFile& assetFile,
    u32 mipmap, u32 layer, io::AssetIoInterface& io, io::AssetCompressor& compressor);
io::IoResult<ImageAsset> readImage(const AssetFile& assetFile,
    io::AssetIoInterface& io, io::AssetCompressor& compressor);

io::IoResult<AssetPacked> pack(const ImageAsset& image, io::AssetCompressor& compressor);
}
}
