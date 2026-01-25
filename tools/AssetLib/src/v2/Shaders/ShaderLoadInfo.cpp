#include "ShaderLoadInfo.h"

#include "v2/Format/ImageFormat.inl"
#include "Utils/HashFileUtils.h"
#include "Utils/HashUtils.h"

#include <glaze/glaze.hpp>

template <>
struct glz::meta<lux::assetlib::ShaderRasterizationDynamicState> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ShaderRasterizationDynamicState;
    static constexpr auto value = glz::enumerate(None, Viewport, Scissor, DepthBias);
};
template <>
struct glz::meta<lux::assetlib::ShaderRasterizationDepthMode> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ShaderRasterizationDepthMode;
    static constexpr auto value = glz::enumerate(Read, ReadWrite, None);
};
template <>
struct glz::meta<lux::assetlib::ShaderRasterizationDepthTest> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ShaderRasterizationDepthTest;
    static constexpr auto value = glz::enumerate(GreaterOrEqual, Equal);
};
template <>
struct glz::meta<lux::assetlib::ShaderRasterizationFaceCullMode> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ShaderRasterizationFaceCullMode;
    static constexpr auto value = glz::enumerate(Front, Back, None);
};
template <>
struct glz::meta<lux::assetlib::ShaderRasterizationPrimitiveKind> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ShaderRasterizationPrimitiveKind;
    static constexpr auto value = glz::enumerate(Triangle, Point);
};
template <>
struct glz::meta<lux::assetlib::ShaderRasterizationAlphaBlending> : lux::assetlib::reflection::CamelCase {
    using enum lux::assetlib::ShaderRasterizationAlphaBlending;
    static constexpr auto value = glz::enumerate(None, Over);
};
template <> struct glz::meta<lux::assetlib::ShaderLoadRasterizationColor> : lux::assetlib::reflection::CamelCase {}; 
template <> struct glz::meta<lux::assetlib::ShaderLoadRasterizationInfo> : lux::assetlib::reflection::CamelCase {}; 
template <> struct glz::meta<lux::assetlib::ShaderLoadInfo::EntryPoint> : lux::assetlib::reflection::CamelCase {};
template <> struct glz::meta<lux::assetlib::ShaderLoadInfo::Variant> : lux::assetlib::reflection::CamelCase {};
template <> struct glz::meta<lux::assetlib::ShaderLoadInfo> : lux::assetlib::reflection::CamelCase {};

namespace 
{
void calculateShaderVariantHashes(lux::assetlib::ShaderLoadInfo& shaderLoadInfo)
{
    for (auto& variant : shaderLoadInfo.Variants)
    {
        variant.NameHash = Hash::string(variant.Name);
        for (auto&& [defineName, defineValue] : variant.Defines)
            Hash::combine(variant.DefinesHash, Hash::string(defineName) ^ Hash::string(defineValue));
    }
}
}

namespace lux::assetlib::shader
{
io::IoResult<ShaderLoadInfo> readLoadInfo(const std::filesystem::path& path)
{
    using namespace io;

    ShaderLoadInfo loadInfo = {};
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    ASSETLIB_CHECK_RETURN_IO_ERROR(in.good(), IoError::ErrorCode::FailedToOpen,
        "ShaderLoadInfo: Failed to open: {}", path.string())
    const isize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::string buffer(size, 0);
    in.read(buffer.data(), size);
    in.close();
    
    const glz::error_ctx error = glz::read_json(loadInfo, buffer);
    ASSETLIB_CHECK_RETURN_IO_ERROR(!error, IoError::ErrorCode::GeneralError,
        "ShaderLoadInfo: Failed to parse: {} ({})", glz::format_error(error, buffer), path.string())

    calculateShaderVariantHashes(loadInfo);
    if (loadInfo.Variants.empty())
        loadInfo.Variants.push_back(ShaderLoadInfo::Variant::MainVariant());
    
    return loadInfo;
}
}
