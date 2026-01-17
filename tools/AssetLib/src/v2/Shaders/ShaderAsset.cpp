#include "ShaderAsset.h"

#include "utils.h"
#include "v2/AssetTypes.h"

#include "v2/Reflection/AssetLibReflectionUtility.inl"

template <> struct ::glz::meta<assetlib::ShaderBinding> : assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<assetlib::ShaderBindingSet> : assetlib::reflection::CamelCase {
    using T = assetlib::ShaderBindingSet;
    static constexpr auto READ_U = [](T& set, glz::raw_json&& input) { set.UniformType = std::move(input.str); };
    static constexpr auto WRITE_U = [](auto& set) -> glz::raw_json_view { return set.UniformType; };
    
    static constexpr auto value = glz::object(
        "set", &T::Set,
        "bindings", &T::Bindings,
        "uniformType", glz::custom<READ_U, WRITE_U>
    );
};
template <> struct ::glz::meta<assetlib::ShaderEntryPoint> : assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<assetlib::ShaderPushConstant> : assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<assetlib::ShaderSpecializationConstants> : assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<assetlib::ShaderInputAttribute> : assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<assetlib::ShaderHeader> : assetlib::reflection::CamelCase {};

namespace assetlib::shader
{
namespace 
{
constexpr u32 SHADER_ASSET_VERSION = 1;
}

AssetMetadata generateMetadata(std::string_view fileName)
{
    return {
        .Type = ASSET_TYPE_SHADER,
        .TypeName = std::string(ASSET_TYPE_SHADER_NAME),
        .Version = SHADER_ASSET_VERSION,
        .OriginalFile = std::string{fileName},
    };
}

io::IoResult<ShaderHeader> unpackHeader(const AssetFile& assetFile)
{
    const auto result = glz::read_json<ShaderHeader>(assetFile.AssetSpecificInfo);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to unpack: {}", glz::format_error(result.error(), assetFile.AssetSpecificInfo))

    return *result;
}

io::IoResult<AssetBinary> unpackBinary(const AssetFile& assetFile, const AssetBinary& assetBinary)
{
    return utils::unpack(assetBinary, assetFile.IoInfo.BinarySizeBytes, assetFile.IoInfo.CompressionMode); 
}

io::IoResult<AssetCustomHeaderType> packHeader(const ShaderHeader& shaderHeader)
{
    auto header = glz::write_json(shaderHeader);
    ASSETLIB_CHECK_RETURN_IO_ERROR(header.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(header.error()))

    return *header;
}

AssetBinary packBinary(AssetBinary& spirv, CompressionMode compressionMode)
{
    return utils::pack(spirv, compressionMode);
}
}
