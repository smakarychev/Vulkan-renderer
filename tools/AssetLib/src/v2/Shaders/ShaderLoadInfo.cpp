#include "ShaderLoadInfo.h"

#include <glaze/glaze.hpp>

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
