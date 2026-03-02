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
    IMAGE_ASSET_RAW_JPG_EXTENSION,
    IMAGE_ASSET_RAW_JPEG_EXTENSION,
    IMAGE_ASSET_RAW_PNG_EXTENSION,
    IMAGE_ASSET_RAW_HDR_EXTENSION,
};

static constexpr std::string_view MATERIAL_ASSET_EXTENSION = ".mat";

static constexpr std::string_view SCENE_ASSET_EXTENSION = ".scene";
static constexpr std::string_view SCENE_ASSET_RAW_GLTF_EXTENSION = ".gltf";
static constexpr std::string_view SCENE_ASSET_RAW_GLB_EXTENSION = ".glb";
static constexpr std::array SCENE_ASSET_RAW_EXTENSIONS = {
    SCENE_ASSET_RAW_GLTF_EXTENSION,
    SCENE_ASSET_RAW_GLB_EXTENSION,
};
}
