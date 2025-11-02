#include "ShaderLoadInfo.h"

#include "ShaderImageFormat.inl"

#include <glaze/glaze.hpp>

template <>
struct glz::meta<assetlib::ShaderRasterizationDynamicState> : assetlib::reflection::CamelCase {
    using enum assetlib::ShaderRasterizationDynamicState;
    static constexpr auto value = glz::enumerate(None, Viewport, Scissor, DepthBias);
};
template <>
struct glz::meta<assetlib::ShaderRasterizationDepthMode> : assetlib::reflection::CamelCase {
    using enum assetlib::ShaderRasterizationDepthMode;
    static constexpr auto value = glz::enumerate(Read, ReadWrite, None);
};
template <>
struct glz::meta<assetlib::ShaderRasterizationDepthTest> : assetlib::reflection::CamelCase {
    using enum assetlib::ShaderRasterizationDepthTest;
    static constexpr auto value = glz::enumerate(GreaterOrEqual, Equal);
};
template <>
struct glz::meta<assetlib::ShaderRasterizationFaceCullMode> : assetlib::reflection::CamelCase {
    using enum assetlib::ShaderRasterizationFaceCullMode;
    static constexpr auto value = glz::enumerate(Front, Back, None);
};
template <>
struct glz::meta<assetlib::ShaderRasterizationPrimitiveKind> : assetlib::reflection::CamelCase {
    using enum assetlib::ShaderRasterizationPrimitiveKind;
    static constexpr auto value = glz::enumerate(Triangle, Point);
};
template <>
struct glz::meta<assetlib::ShaderRasterizationAlphaBlending> : assetlib::reflection::CamelCase {
    using enum assetlib::ShaderRasterizationAlphaBlending;
    static constexpr auto value = glz::enumerate(None, Over);
};
template <> struct glz::meta<assetlib::ShaderLoadRasterizationInfo> : assetlib::reflection::CamelCase {}; 
template <> struct glz::meta<assetlib::ShaderLoadInfo::EntryPoint> : assetlib::reflection::CamelCase {}; 
template <> struct glz::meta<assetlib::ShaderLoadInfo> : assetlib::reflection::CamelCase {}; 

namespace assetlib::shader
{
io::IoResult<ShaderLoadInfo> readLoadInfo(const std::filesystem::path& path)
{
    using namespace io;

    ShaderLoadInfo loadInfo = {};
    const glz::error_ctx error = glz::read_file_json(loadInfo, path.string(), std::string{});
    ASSETLIB_CHECK_RETURN_IO_ERROR(!error, IoError::ErrorCode::GeneralError,
        "ShaderLoadInfo: Failed to parse: {} ({})", glz::format_error(error), path.string())
    
    return loadInfo;
}
}
