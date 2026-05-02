#include "ImageAsset.h"

#include "ImageMeta.h"

#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>

#include <CoreLib/Utils/FileUtils.h>

template <>
struct glz::meta<lux::assetlib::ImageKind> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ImageKind;
    static constexpr auto value = glz::enumerate(Image2d, Image3d, ImageCubemap, Image2dArray);
};
template <> struct ::glz::meta<lux::assetlib::ImageHeader> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::image
{
io::IoResult<ImageHeader> readHeader(const AssetMetadata& metadata)
{
    auto headerRead = readFileToString(metadata.Io.HeaderFile);
    ASSETLIB_CHECK_RETURN_IO_ERROR(headerRead.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read header file: {}", metadata.Io.HeaderFile.string())
    
    const auto result = glz::read_json<ImageHeader>(*headerRead);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: {}", glz::format_error(result.error(), *headerRead))

    return *result;
}

io::IoResult<std::vector<std::byte>> readImageData(const ImageHeader& header, const AssetMetadata& metadata,
    u32 mipmap, u32 layer, io::AssetIoInterface& io, io::AssetCompressor& compressor)
{
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.Mipmaps > mipmap, io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: specified mipmap is not available {} (total {})", mipmap, header.Mipmaps)
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.Layers > layer, io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: specified layer is not available {} (total {})", layer, header.Layers)

    const u32 imageDataCompressedSizeIndex = mipmap * header.Layers + layer;
    u64 dataOffset = 0;
    for (u32 i = 0; i < imageDataCompressedSizeIndex; i++)
        dataOffset += metadata.Io.BinarySizeBytesChunksCompressed[i];
    const u64 dataSize = metadata.Io.BinarySizeBytesChunksCompressed[imageDataCompressedSizeIndex];
    
    std::vector<std::byte> imageData(dataSize);
    auto result = io.ReadBinaryChunk(metadata, imageData.data(), dataOffset, imageData.size());
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read: {} ({})", result.error(), metadata.Io.BinaryFile.string())

    imageData = compressor.Decompress(imageData, header.MipmapSizes[mipmap][layer]);

    return imageData;
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
        .Header = std::move(*header),
        .PackedBinaries = std::move(imageData),
        .PackedBinarySizeBytesChunks = std::move(packedImageDataBinarySizeBytesChunks)
    };
}
}
