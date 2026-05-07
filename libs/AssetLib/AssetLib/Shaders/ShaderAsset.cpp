#include "ShaderAsset.h"

#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>

#include <CoreLib/Utils/FileUtils.h>

template <> struct ::glz::meta<lux::assetlib::ShaderBinding> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::ShaderBindingSet> : lux::assetlib::reflection::CamelCase {
    using T = lux::assetlib::ShaderBindingSet;
    static constexpr auto READ_U = [](T& set, glz::raw_json&& input) { set.UniformType = std::move(input.str); };
    static constexpr auto WRITE_U = [](auto& set) -> glz::raw_json_view { return set.UniformType; };
    
    static constexpr auto value = glz::object(
        "set", &T::Set,
        "bindings", &T::Bindings,
        "uniformType", glz::custom<READ_U, WRITE_U>
    );
};
template <> struct ::glz::meta<lux::assetlib::ShaderEntryPoint> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::ShaderPushConstant> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::ShaderSpecializationConstants> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::ShaderInputAttribute> : lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::ShaderHeader> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::shader
{
io::IoResult<ShaderHeader> readHeader(const AssetMetadata& metadata)
{
    DEFINE_BASIC_HEADER_READ(ShaderHeader, result, metadata)
    return *result;
}

io::IoResult<std::vector<std::byte>> readSpirv(const ShaderHeader&, const AssetMetadata& metadata,
    io::AssetIoInterface& io, io::AssetCompressor& compressor)
{
    std::vector<std::byte> spirv(metadata.Io.BinarySizeBytesCompressed);
    auto result = io.ReadBinaryChunk(metadata, spirv.data(), 0, spirv.size());
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read: {} ({})", result.error(), metadata.Io.BinaryFile.string())

    spirv = compressor.Decompress(spirv, metadata.Io.BinarySizeBytes);
    
    return spirv;
}

io::IoResult<AssetPacked> pack(const ShaderAsset& shader, io::AssetCompressor& compressor)
{
    auto header = glz::write_json(shader.Header);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    auto spirv = compressor.Compress(shader.Spirv);
    const u64 spirvSize = spirv.size();

    return AssetPacked{
        .Header = std::move(*header),
        .PackedBinaries = std::move(spirv),
        .PackedBinarySizeBytesChunks = {spirvSize}
    };
}
}
