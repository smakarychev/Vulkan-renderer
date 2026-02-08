#include "ImageLoadInfo.h"

#include "v2/Format/ImageFormat.inl"
template <> struct glz::meta<lux::assetlib::ImageLoadInfo> : lux::assetlib::reflection::CamelCase {};

namespace lux::assetlib::image
{
io::IoResult<ImageLoadInfo> readLoadInfo(const std::filesystem::path& path)
{
    using namespace io;

    ImageLoadInfo loadInfo = {};
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    ASSETLIB_CHECK_RETURN_IO_ERROR(in.good(), IoError::ErrorCode::FailedToOpen,
        "ImageLoadInfo: Failed to open: {}", path.string())
    const isize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::string buffer(size, 0);
    in.read(buffer.data(), size);
    in.close();
    
    const glz::error_ctx error = glz::read_json(loadInfo, buffer);
    ASSETLIB_CHECK_RETURN_IO_ERROR(!error, IoError::ErrorCode::GeneralError,
        "ImageLoadInfo: Failed to parse: {} ({})", glz::format_error(error, buffer), path.string())

    return loadInfo;
}

io::IoResult<std::string> packLoadInfo(const ImageLoadInfo& imageLoadInfo)
{
    auto loadInfo = glz::write_json(imageLoadInfo);
    ASSETLIB_CHECK_RETURN_IO_ERROR(loadInfo.has_value(), io::IoError::ErrorCode::GeneralError,
        "Assetlib: Failed to pack: {}", glz::format_error(loadInfo.error()))

    return *loadInfo;
}
}
