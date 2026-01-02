#include "ShaderLoadInfo.h"

#include "ShaderImageFormat.inl"
#include "Utils/HashFileUtils.h"
#include "Utils/HashUtils.h"

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
template <> struct glz::meta<assetlib::ShaderLoadRasterizationColor> : assetlib::reflection::CamelCase {}; 
template <> struct glz::meta<assetlib::ShaderLoadRasterizationInfo> : assetlib::reflection::CamelCase {}; 
template <> struct glz::meta<assetlib::ShaderLoadInfo::EntryPoint> : assetlib::reflection::CamelCase {};
template <> struct glz::meta<assetlib::ShaderLoadInfo::Variant> : assetlib::reflection::CamelCase {};
template <> struct glz::meta<assetlib::ShaderLoadInfo> : assetlib::reflection::CamelCase {};

namespace 
{
void calculateShaderVariantHashes(assetlib::ShaderLoadInfo& shaderLoadInfo)
{
    for (auto& variant : shaderLoadInfo.Variants)
    {
        variant.NameHash = Hash::string(variant.Name);
        for (auto&& [defineName, defineValue] : variant.Defines)
            Hash::combine(variant.DefinesHash, Hash::string(defineName) ^ Hash::string(defineValue));
    }
}
}

namespace assetlib::shader
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
