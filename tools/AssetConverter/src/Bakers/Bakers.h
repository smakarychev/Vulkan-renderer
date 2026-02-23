#pragma once
#include <string_view>
#include <array>

namespace lux::bakers
{
static constexpr std::string_view SHADER_ASSET_EXTENSION = ".shader";
static constexpr std::string_view SHADER_ASSET_RAW_EXTENSION = ".slang";


static constexpr std::string_view IMAGE_ASSET_EXTENSION = ".tex";
static constexpr std::string_view IMAGE_ASSET_RAW_JPG_EXTENSION = ".jpg";
static constexpr std::string_view IMAGE_ASSET_RAW_JPEG_EXTENSION = ".jpeg";
static constexpr std::string_view IMAGE_ASSET_RAW_PNG_EXTENSION = ".png";
static constexpr std::string_view IMAGE_ASSET_RAW_HDR_EXTENSION = ".hdr";
static constexpr std::array IMAGE_ASSET_RAW_EXTENSIONS = {
    (std::string_view)IMAGE_ASSET_RAW_JPG_EXTENSION,
    (std::string_view)IMAGE_ASSET_RAW_JPEG_EXTENSION,
    (std::string_view)IMAGE_ASSET_RAW_PNG_EXTENSION,
    (std::string_view)IMAGE_ASSET_RAW_HDR_EXTENSION
};
}
