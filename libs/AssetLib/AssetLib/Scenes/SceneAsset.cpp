#include "SceneAsset.h"

#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>

template <>
struct glz::meta<lux::assetlib::SceneAssetAccessorComponentType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::SceneAssetAccessorComponentType;
    static constexpr auto value = glz::enumerate(U8, U16, U32, F32, Meshlet);
};
template <>
struct glz::meta<lux::assetlib::SceneAssetAccessorType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::SceneAssetAccessorType;
    static constexpr auto value = glz::enumerate(Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4);
};
template <> struct ::glz::meta<lux::assetlib::SceneAssetAccessor> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetBuffer> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetBufferView> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetPrimitive::Attribute> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetPrimitive> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetMesh> : lux::assetlib::reflection::CamelCase {};
template <>
struct glz::meta<lux::assetlib::SceneAssetTextureFilter> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::SceneAssetTextureFilter;
    static constexpr auto value = glz::enumerate(Linear, Nearest);
};
template <> struct ::glz::meta<lux::assetlib::SceneAssetTextureSample> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetMaterial> : lux::assetlib::reflection::CamelCase {};
template <>
struct glz::meta<lux::assetlib::SceneAssetCameraType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::SceneAssetCameraType;
    static constexpr auto value = glz::enumerate(Perspective, Orthographic);
};
template <> struct ::glz::meta<lux::assetlib::SceneAssetCamera::PerspectiveData>
    : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetCamera::OrthographicData>
    : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetCamera> : lux::assetlib::reflection::CamelCase {};
template <>
struct glz::meta<lux::assetlib::SceneAssetLightType> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::SceneAssetLightType;
    static constexpr auto value = glz::enumerate(Directional, Point, Spot);
};
template <> struct ::glz::meta<lux::assetlib::SceneAssetLight> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetMeshlet> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetNode> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetSubscene> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::SceneAssetHeader> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::scene
{
io::IoResult<SceneAssetHeader> readHeader(const AssetFile& assetFile)
{
    const auto result = glz::read_json<SceneAssetHeader>(assetFile.AssetSpecificInfo);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: {}", glz::format_error(result.error(), assetFile.AssetSpecificInfo))

    return *result;
}

io::IoResult<std::vector<std::byte>> readBufferData(const SceneAssetHeader& header, const AssetFile& assetFile,
    u32 bufferIndex, io::AssetIoInterface& io, io::AssetCompressor& compressor)
{
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.Buffers.size() > bufferIndex, io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: specified buffer is not available {} (total {})", bufferIndex, header.Buffers.size())

    u64 dataOffset = 0;
    for (u32 i = 0; i < bufferIndex; i++)
        dataOffset += assetFile.IoInfo.BinarySizeBytesChunksCompressed[i];
    const u64 dataSize = assetFile.IoInfo.BinarySizeBytesChunksCompressed[bufferIndex];
    
    std::vector<std::byte> bufferData(dataSize);
    auto result = io.ReadBinaryChunk(assetFile, bufferData.data(), dataOffset, bufferData.size());
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read: {} ({})", result.error(), assetFile.IoInfo.BinaryFile.string())

    bufferData = compressor.Decompress(bufferData, header.Buffers[bufferIndex].SizeBytes);

    return bufferData;
}

io::IoResult<SceneAsset> readScene(const AssetFile& assetFile, io::AssetIoInterface& io,
    io::AssetCompressor& compressor)
{
    auto header = readHeader(assetFile);
    if (!header.has_value())
        return std::unexpected(header.error());

    SceneAsset asset = {
        .Header = std::move(*header)
    };
    asset.BuffersData.resize(asset.Header.Buffers.size());

    for (u32 buffer = 0; buffer < asset.BuffersData.size(); buffer++)
    {
        auto binary = readBufferData(asset.Header, assetFile, buffer, io, compressor);
        ASSETLIB_CHECK_RETURN_IO_ERROR(binary.has_value(), io::IoError::ErrorCode::FailedToLoad,
            "Assetlib: Failed to read: {} ({})", binary.error(), assetFile.IoInfo.BinaryFile.string())
        
        asset.BuffersData[buffer] = std::move(*binary);
    }

    return asset;
}

io::IoResult<AssetPacked> pack(const SceneAsset& scene, io::AssetCompressor& compressor)
{
    auto header = glz::write_json(scene.Header);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    std::vector<u64> packedBufferDataBinarySizeBytesChunks(scene.Header.Buffers.size());
    std::vector<std::byte> bufferData;

    for (u32 buffer = 0; buffer < scene.Header.Buffers.size(); buffer++)
    {
        auto compressed = compressor.Compress(scene.BuffersData[buffer]);
        packedBufferDataBinarySizeBytesChunks[buffer] = compressed.size();
        
        bufferData.append_range(std::move(compressed));
    }
    
    return AssetPacked{
        .Metadata = getMetadata(),
        .AssetSpecificInfo = std::move(*header),
        .PackedBinaries = std::move(bufferData),
        .PackedBinarySizeBytesChunks = std::move(packedBufferDataBinarySizeBytesChunks)
    };
}

AssetMetadata getMetadata()
{
    static constexpr u32 SCENE_ASSET_VERSION = 1;
    
    return {
        .Type = "7b331c56-5ad1-4269-8737-7a6c0a9f46da"_guid,
        .TypeName = "scene",
        .Version = SCENE_ASSET_VERSION,
    };
}
}

lux::assetlib::SceneAssetPrimitive::Attribute* lux::assetlib::SceneAssetPrimitive::FindAttribute(
    std::string_view name)
{
    auto it = std::ranges::find_if(Attributes, [&](auto& attribute) { return attribute.Name == name; });
    
    return it == Attributes.end() ? nullptr : &*it;
}
