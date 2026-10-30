#include "AssetPackageMetadata.h"

#include <AssetLib/Reflection/AssetlibReflection.h>
#include <AssetLib/Reflection/AssetlibReflectionUtility.inl>

template <>
struct glz::meta<lux::assetlib::AssetPackageMetadata2::ImporterInfo> {
    using T = lux::assetlib::AssetPackageMetadata2::ImporterInfo;

    static constexpr auto READ_INFO = [](T& self, const raw_json& raw) {
        self.Info = raw.str;
    };

    static constexpr auto WRITE_INFO = [](const T& self) -> raw_json {
        return self.Info.empty() ? raw_json{"{}"} : raw_json{self.Info};
    };

    static constexpr auto value = object(
        "importerType", &T::ImporterType,
        "info", glz::custom<READ_INFO, WRITE_INFO>
    );
};

template <> struct ::glz::meta<lux::assetlib::AssetPackageMetadata2::AssetMeta> :
    lux::assetlib::reflection::CamelCase {};
template <> struct ::glz::meta<lux::assetlib::AssetPackageMetadata2> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib
{
io::IoResult<AssetPackageMetadata2> AssetPackageMetadata2::FromString(const std::string& buffer)
{
    AssetPackageMetadata2 metadata;
    const glz::error_ctx error = glz::read_json(metadata, buffer);
    ASSETLIB_CHECK_RETURN_IO_ERROR(!error, io::IoError::ErrorCode::GeneralError, "Failed to parse: {} ({})",
        glz::format_error(error, buffer), buffer)
    
    return metadata;
}

std::string AssetPackageMetadata2::ToString() const
{
    auto stringJson = glz::write_json(*this);
    
    return stringJson.value_or("");
}

std::string AssetPackageMetadata2::ToFormattedString() const
{
    return glz::prettify_json(ToString());
}
}
