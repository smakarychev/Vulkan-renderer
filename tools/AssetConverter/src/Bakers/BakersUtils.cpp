#include "BakersUtils.h"

#include "BakerContext.h"
#include "core.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"

namespace lux::bakers
{
namespace 
{
constexpr std::string_view BAKED_ASSETS_DIRECTORY_NAME = "baked";
}
std::filesystem::path getPostBakePath(const std::filesystem::path& path, const Context& ctx)
{
    std::filesystem::path processedPath = path.filename();

    std::filesystem::path currentPath = path.parent_path();
    while (!std::filesystem::equivalent(currentPath, ctx.InitialDirectory))
    {
        processedPath = currentPath.filename() / processedPath;
        currentPath = currentPath.parent_path();
    }

    processedPath = currentPath / BAKED_ASSETS_DIRECTORY_NAME / processedPath;

    return processedPath;
}

AssetPaths getPostBakePaths(const std::filesystem::path& path, const Context& ctx, std::string_view postBakeExtension,
    const assetlib::io::AssetIoInterface& io)
{
    const std::filesystem::path processedPath = getPostBakePath(path, ctx);
    auto headerPath = processedPath;
    auto binaryPath = processedPath;

    headerPath.replace_extension(io.GetHeaderExtension(postBakeExtension));
    binaryPath.replace_extension(io.GetBinariesExtension());

    return {
        .HeaderPath = std::move(headerPath),
        .BinaryPath = std::move(binaryPath),
    };
}
}
