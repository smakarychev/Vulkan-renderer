#include "ShaderAsset.h"

#include "utils.h"
#include "v2/Io/Compression/AssetCompressor.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"

#include "v2/Reflection/AssetLibReflectionUtility.inl"

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
namespace 
{
constexpr u32 SHADER_ASSET_VERSION = 1;
AssetMetadata generateMetadata()
{
    return {
        .Type = "fb8f866d-d436-48d3-beef-2cd3dca6e0c8"_guid,
        .TypeName = "shader",
        .Version = SHADER_ASSET_VERSION,
    };
}
}


io::IoResult<ShaderHeader> readHeader(const AssetFile& assetFile)
{
    const auto result = glz::read_json<ShaderHeader>(assetFile.AssetSpecificInfo);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to read: {}", glz::format_error(result.error(), assetFile.AssetSpecificInfo))

    return *result;
}

io::IoResult<std::vector<std::byte>> readSpirv(const ShaderHeader&, const AssetFile& assetFile,
    io::AssetIoInterface& io, io::AssetCompressor& compressor)
{
    std::vector<std::byte> spirv(assetFile.IoInfo.BinarySizeBytesCompressed);
    auto result = io.ReadBinaryChunk(assetFile, spirv.data(), 0, spirv.size());
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read: {} ({})", result.error(), assetFile.IoInfo.BinaryFile.string())

    spirv = compressor.Decompress(spirv, assetFile.IoInfo.BinarySizeBytes);
    
    return spirv;
}

io::IoResult<ShaderAsset> readShader(const AssetFile& assetFile, io::AssetIoInterface& io,
    io::AssetCompressor& compressor)
{
    auto header = readHeader(assetFile);
    if (!header.has_value())
        return std::unexpected(header.error());

    auto binary = readSpirv(*header, assetFile, io, compressor);
    ASSETLIB_CHECK_RETURN_IO_ERROR(binary.has_value(), io::IoError::ErrorCode::FailedToLoad,
        "Assetlib: Failed to read: {} ({})", binary.error(), assetFile.IoInfo.BinaryFile.string())

    return ShaderAsset{
        .Header = std::move(*header),
        .Spirv = std::move(*binary),
    };
}

io::IoResult<AssetPacked> pack(const ShaderAsset& shader, io::AssetCompressor& compressor)
{
    auto header = glz::write_json(shader.Header);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    auto spirv = compressor.Compress(shader.Spirv);
    const u64 spirvSize = spirv.size();

    return AssetPacked{
        .Metadata = generateMetadata(),
        .AssetSpecificInfo = std::move(*header),
        .PackedBinaries = std::move(spirv),
        .PackedBinarySizeBytesChunks = {spirvSize}
    };
}
}
