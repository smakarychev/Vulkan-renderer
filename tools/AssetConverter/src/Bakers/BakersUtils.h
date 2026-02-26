#pragma once
#include "v2/AssetLibV2.h"

#include <filesystem>

namespace lux::assetlib::io
{
class AssetIoInterface;
}

namespace lux::bakers
{
struct Context;
struct AssetPaths
{
    std::filesystem::path HeaderPath{};
    std::filesystem::path BinaryPath{};
};
std::filesystem::path getPostBakePath(const std::filesystem::path& path, const Context& ctx);
AssetPaths getPostBakePaths(const std::filesystem::path& path, const Context& ctx, std::string_view postBakeExtension,
    const assetlib::io::AssetIoInterface& io);
assetlib::AssetId getBakedAssetId(const std::filesystem::path& bakedPath, assetlib::io::AssetIoInterface& io);
}
