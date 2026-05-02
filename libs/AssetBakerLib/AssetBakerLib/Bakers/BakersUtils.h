#pragma once
#include <AssetLib/Assetlib.h>

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

std::filesystem::path getPostBakePath(const assetlib::AssetMetadata& metadata, std::string_view postBakeExtension,
    const Context& ctx);
AssetPaths getPostBakePaths(const assetlib::AssetMetadata& metadata, std::string_view postBakeExtension,
    const Context& ctx);
}
