#include "ImageAsset.h"

#include "utils.h"
#include "v2/Io/Compression/AssetCompressor.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"
#include "v2/Reflection/AssetLibReflectionUtility.inl"
#include "v2/Format/ImageFormat.inl"

template <>
struct glz::meta<lux::assetlib::ImageKind> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ImageKind;
    static constexpr auto value = glz::enumerate(Image2d, Image3d, ImageCubemap, Image2dArray);
};
template <>
struct glz::meta<lux::assetlib::ImageFilter> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ImageFilter;
    static constexpr auto value = glz::enumerate(Linear, Nearest);
};
template <> struct ::glz::meta<lux::assetlib::ImageHeader> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::image
{
io::IoResult<ImageHeader> readHeader(const AssetFile& assetFile)
{
    const auto result = glz::read_json<ImageHeader>(assetFile.AssetSpecificInfo);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: {}", glz::format_error(result.error(), assetFile.AssetSpecificInfo))

    return *result;
}

io::IoResult<std::vector<std::byte>> readImageData(const ImageHeader& header, const AssetFile& assetFile,
    u32 mipmap, u32 layer, io::AssetIoInterface& io, io::AssetCompressor& compressor)
{
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.Mipmaps > mipmap, io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: specified mipmap is not available {} (total {})", mipmap, header.Mipmaps)
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.Layers > layer, io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: specified layer is not available {} (total {})", layer, header.Layers)

    const u32 imageDataCompressedSizeIndex = mipmap * header.Layers + layer;
    u64 dataOffset = 0;
    for (u32 i = 0; i < imageDataCompressedSizeIndex; i++)
        dataOffset += assetFile.IoInfo.BinarySizeBytesChunksCompressed[i];
    u64 dataSize = assetFile.IoInfo.BinarySizeBytesChunksCompressed[imageDataCompressedSizeIndex];
    
    std::vector<std::byte> imageData(dataSize);
    auto result = io.ReadBinaryChunk(assetFile, imageData.data(), dataOffset, imageData.size());
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read: {} ({})", result.error(), assetFile.IoInfo.BinaryFile.string())

    imageData = compressor.Decompress(imageData, header.MipmapSizes[mipmap][layer]);

    return imageData;
}

io::IoResult<ImageAsset> readImage(const AssetFile& assetFile, io::AssetIoInterface& io,
    io::AssetCompressor& compressor)
{
    auto header = readHeader(assetFile);
    if (!header.has_value())
        return std::unexpected(header.error());

    ImageAsset asset = {
        .Header = std::move(*header)
    };
    asset.MipmapsImageData.resize(header->Mipmaps);
    for (auto& mip : asset.MipmapsImageData)
        mip.resize(header->Layers);
    
    for (u32 mip = 0; mip < header->Mipmaps; mip++)
    {
        for (u32 layer = 0; layer < header->Layers; layer++)
        {
            auto binary = readImageData(asset.Header, assetFile, mip, layer, io, compressor);
            ASSETLIB_CHECK_RETURN_IO_ERROR(binary.has_value(), io::IoError::ErrorCode::FailedToLoad,
                "Assetlib: Failed to read: {} ({})", binary.error(), assetFile.IoInfo.BinaryFile.string())
            
            asset.MipmapsImageData[mip][layer] = std::move(*binary);
        }
    }

    return asset;
}

io::IoResult<AssetPacked> pack(const ImageAsset& image, io::AssetCompressor& compressor)
{
    auto header = glz::write_json(image.Header);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    std::vector<u64> packedImageDataBinarySizeBytesChunks((u64)image.Header.Mipmaps * image.Header.Layers);
    std::vector<std::byte> imageData;

    for (u32 mip = 0; mip < image.Header.Mipmaps; mip++)
    {
        for (u32 layer = 0; layer < image.Header.Layers; layer++)
        {
            const u32 flatIndex = mip * image.Header.Layers + layer;
            auto compressed = compressor.Compress(image.MipmapsImageData[mip][layer]);
            packedImageDataBinarySizeBytesChunks[flatIndex] = compressed.size();
            
            imageData.append_range(std::move(compressed));
        }
    }

    return AssetPacked{
        .Metadata = getMetadata(),
        .AssetSpecificInfo = std::move(*header),
        .PackedBinaries = std::move(imageData),
        .PackedBinarySizeBytesChunks = std::move(packedImageDataBinarySizeBytesChunks)
    };
}

AssetMetadata getMetadata()
{
    static constexpr u32 IMAGE_ASSET_VERSION = 1;
    
    return {
        .Type = "66c40884-12d6-4f3f-a801-34d585c692a3"_guid,
        .TypeName = "image",
        .Version = IMAGE_ASSET_VERSION,
    };
}
}
