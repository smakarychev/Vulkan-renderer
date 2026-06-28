#include "GeometryBufferAsset.h"

#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>
#include <CoreLib/Utils/FileUtils.h>

template <>
struct glz::meta<lux::assetlib::GeometryBufferAccessorComponentType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::GeometryBufferAccessorComponentType;
    static constexpr auto value = glz::enumerate(U8, U16, U32, F32, Meshlet);
};
template <>
struct glz::meta<lux::assetlib::GeometryBufferAccessorType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::GeometryBufferAccessorType;
    static constexpr auto value = glz::enumerate(Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4);
};
template <> struct ::glz::meta<lux::assetlib::GeometryBufferAccessor::SparseAccessor::IndicesAccessor>
    : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::GeometryBufferAccessor::SparseAccessor::DataAccessor> 
    : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::GeometryBufferAccessor::SparseAccessor> 
    : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::GeometryBufferAccessor> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::GeometryBufferView> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::GeometryBufferHeader> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::sceneGeometry
{
io::IoResult<GeometryBufferHeader> readHeader(const AssetMetadata& metadata)
{
    DEFINE_BASIC_HEADER_READ(GeometryBufferHeader, result, metadata)
    return *result;
}

io::IoResult<std::vector<std::byte>> readBufferData(const GeometryBufferHeader& header, const AssetMetadata& metadata,
    io::AssetIoInterface& io, io::AssetCompressor& compressor)
{
    constexpr u64 dataOffset = 0;
    const u64 dataSize = metadata.Io.BinarySizeBytesCompressed;
    
    std::vector<std::byte> bufferData(dataSize);
    auto result = io.ReadBinaryChunk(metadata, bufferData.data(), dataOffset, bufferData.size());
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read: {} ({})", result.error(), metadata.Io.BinaryFile.string())

    bufferData = compressor.Decompress(bufferData, header.SizeBytes);

    return bufferData;
}

io::IoResult<AssetPacked> pack(const GeometryBufferAsset& geometry, io::AssetCompressor& compressor)
{
    auto header = glz::write_json(geometry.Header);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    auto compressed = compressor.Compress(geometry.Data);
    u64 compressedSize = compressed.size();
    
    return AssetPacked{
        .Header = std::move(*header),
        .PackedBinaries = std::move(compressed),
        .PackedBinarySizeBytesChunks = {compressedSize}
    };
}
}

