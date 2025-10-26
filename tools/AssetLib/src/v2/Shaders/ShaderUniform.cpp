#include "ShaderUniform.h"

#include <glaze/json/write.hpp>
#include "v2/Reflection/AssetLibReflectionUtility.inl"

template <>
struct ::glz::meta<assetlib::ShaderUniformTypeScalar> : assetlib::reflection::CamelCase {};
template <>
struct ::glz::meta<assetlib::ShaderUniformTypeArray> : assetlib::reflection::CamelCase {};
template <>
struct ::glz::meta<assetlib::ShaderUniformTypeVector> : assetlib::reflection::CamelCase {};
template <>
struct ::glz::meta<assetlib::ShaderUniformTypeMatrix> : assetlib::reflection::CamelCase {};
template <>
struct ::glz::meta<assetlib::ShaderUniformTypeVariant> : assetlib::reflection::CamelCase {};
template <>
struct ::glz::meta<assetlib::ShaderUniformType> : assetlib::reflection::CamelCase {};
template <>
struct ::glz::meta<assetlib::ShaderUniformVariable> : assetlib::reflection::CamelCase{};
template <>
struct ::glz::meta<assetlib::ShaderUniformTypeStruct> : assetlib::reflection::CamelCase{};
template <>
struct ::glz::meta<assetlib::ShaderUniformTypeEmbeddedStruct> : assetlib::reflection::CamelCase{};
template <>
struct ::glz::meta<assetlib::ShaderUniformTypeStructReference> : assetlib::reflection::CamelCase{};
template <>
struct ::glz::meta<assetlib::ShaderUniform> : assetlib::reflection::CamelCase{};

namespace assetlib::shader
{
io::IoResult<std::string> packUniformStruct(const ShaderUniformTypeStruct& uniformStruct)
{
    auto result = glz::write_json(uniformStruct);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(result.error()))

    return *result;
}

io::IoResult<ShaderUniformTypeStruct> unpackUniformStruct(const std::string& uniformStruct)
{
    const auto result = glz::read_json<ShaderUniformTypeStruct>(uniformStruct);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to unpack: {}", glz::format_error(result.error(), uniformStruct))

    return *result;
}

io::IoResult<std::string> packUniform(const ShaderUniform& uniform)
{
    auto uniformString = glz::write_json(uniform);
    ASSETLIB_CHECK_RETURN_IO_ERROR(uniformString.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(uniformString.error()))

    return *uniformString;
}

io::IoResult<ShaderUniform> unpackUniform(const std::string& uniform)
{
    auto result = glz::read_json<ShaderUniform>(uniform);
    ASSETLIB_CHECK_RETURN_IO_ERROR(result.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to unpack: {}", glz::format_error(result.error(), uniform))

    return std::move(result.value());
}
}
