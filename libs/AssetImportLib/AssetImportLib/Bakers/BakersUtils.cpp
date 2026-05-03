#include "BakersUtils.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetImportLib/Importers/ImportContext.h>

namespace lux::import
{
std::filesystem::path getPostBakePath(const assetlib::AssetMetadata& metadata, std::string_view postBakeExtension,
    const Context& ctx)
{
    const std::string bakedFileName =
        std::format("{}/{}", metadata.Type.Name, metadata.AssetId);
    
    std::filesystem::path bakedBasePath = ctx.BakedDirectory / bakedFileName;
    bakedBasePath.replace_extension(ctx.Io->GetHeaderExtension(postBakeExtension));
    
    return bakedBasePath;
}

AssetPaths getPostBakePaths(const assetlib::AssetMetadata& metadata, std::string_view postBakeExtension,
    const Context& ctx)
{
    const std::filesystem::path processedPath = getPostBakePath(metadata, postBakeExtension, ctx);
    auto headerPath = processedPath;
    auto binaryPath = processedPath;

    binaryPath.replace_extension(ctx.Io->GetBinariesExtension());

    return {
        .HeaderPath = std::move(headerPath),
        .BinaryPath = std::move(binaryPath),
    };
}
}
