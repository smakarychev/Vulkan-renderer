#include "BakersUtils.h"

#include "BakerContext.h"
#include "core.h"

namespace bakers
{
namespace 
{
constexpr std::string_view RAW_ASSETS_DIRECTORY_NAME = "raw";
constexpr std::string_view BAKED_ASSETS_DIRECTORY_NAME = "baked";
constexpr std::string_view BAKED_ASSETS_BINARY_EXTENSION = "bin";
constexpr std::string_view BAKED_ASSETS_COMBINED_EXTENSION = "gbin";
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
    assetlib::AssetFileIoType ioType)
{
    const std::filesystem::path processedPath = getPostBakePath(path, ctx);
    auto headerPath = processedPath;
    auto binaryPath = processedPath;

    switch (ioType)
    {
    case assetlib::AssetFileIoType::Separate:
        headerPath.replace_extension(postBakeExtension);
        binaryPath.replace_extension(BAKED_ASSETS_BINARY_EXTENSION);
        break;
    case assetlib::AssetFileIoType::Combined:
        headerPath.replace_extension(BAKED_ASSETS_COMBINED_EXTENSION);
        binaryPath.replace_extension(BAKED_ASSETS_COMBINED_EXTENSION);
        break;
    default:
        ASSERT(false, "Unknown io type");
        headerPath.replace_extension(postBakeExtension);
        binaryPath.replace_extension(BAKED_ASSETS_BINARY_EXTENSION);
        break;
    }

    return {
        .HeaderPath = std::move(headerPath),
        .BinaryPath = std::move(binaryPath),
    };
}
}
