#include "BakersUtils.h"

#include "BakerContext.h"
#include "core.h"
#include "v2/Io/IoInterface/AssetIoInterface.h"

namespace lux::bakers
{
namespace 
{
constexpr std::string_view RAW_ASSETS_DIRECTORY_NAME = "raw";
constexpr std::string_view BAKED_ASSETS_DIRECTORY_NAME = "baked";
}
/* this function reflects input path around "raw" directory, if such is present.
 * e.g. `some/raw/path/resource.ext` turns into `some/baked/path/resource.ext`
 */
std::filesystem::path getPostBakePath(const std::filesystem::path& path, const Context& ctx)
{
    std::filesystem::path processedPath = path.filename();

    std::filesystem::path currentPath = path.parent_path();
    while (!std::filesystem::equivalent(currentPath, ctx.InitialDirectory))
    {
        if (currentPath.filename() == RAW_ASSETS_DIRECTORY_NAME)
            break;
        processedPath = currentPath.filename() / processedPath;
        currentPath = currentPath.parent_path();
    }

    if (currentPath.filename() == RAW_ASSETS_DIRECTORY_NAME)
        processedPath = currentPath.parent_path() / BAKED_ASSETS_DIRECTORY_NAME / processedPath;
    else
        processedPath = path.parent_path() / BAKED_ASSETS_DIRECTORY_NAME / processedPath;

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
