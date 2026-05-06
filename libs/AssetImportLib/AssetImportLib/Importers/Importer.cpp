#include "Importer.h"

#include <AssetImportLib/Bakers/BakersUtils.h>
#include <CoreLib/utils/FileUtils.h>

#define CHECK_RETURN_IO_ERROR(x, error, ...) \
ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, __VA_ARGS__)

namespace lux::import
{
ImportResult<void> Importer::Import(const std::filesystem::path& path)
{
    return Import(path, ImportFlags::BakeIfNotBaked | ImportFlags::Header | ImportFlags::Binaries);
}

assetlib::AssetMetadata Importer::CreateMetadataBase(const std::filesystem::path& metaPath,
    const std::filesystem::path& rawPath, const assetlib::AssetTypeMetadata& typeMetadata,
    std::string_view postBakeExtension, const Context& ctx)
{
    const assetlib::AssetId assetId = assetlib::io::readBaseAssetMetadata(metaPath).value_or({}).AssetId;
    assetlib::AssetMetadata metadata {
        .AssetId = assetId,
        .Type = typeMetadata,
        .Io = {
            .OriginalFile = std::filesystem::weakly_canonical(rawPath).generic_string(),
        }
    };
    const AssetPaths paths = getPostBakePaths(metadata, postBakeExtension, ctx);
    metadata.Io.HeaderFile = paths.HeaderPath;
    metadata.Io.BinaryFile = paths.BinaryPath;
    
    return metadata;
}

IoResult<void> Importer::WritePackedMetadata(const std::filesystem::path& metaPath,
    const assetlib::io::IoResult<std::string>& metadata,
    std::string_view typeLabel)
{
    CHECK_RETURN_IO_ERROR(metadata.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to pack {} meta data for {}", typeLabel, metaPath.generic_string())

    auto writeResult = writeStringToFile(metaPath, assetlib::io::getAssetHeaderFormatted(*metadata));
    CHECK_RETURN_IO_ERROR(writeResult.has_value(), IoError::ErrorCode::FailedToCreate,
        "Failed to write {} meta data for {}", typeLabel, metaPath.generic_string())
    
    return {};
}

ImportResult<std::filesystem::path> Importer::EnsureMetadata(const std::filesystem::path& rawPath)
{
    std::filesystem::path metadataPath = GetMetaPath(rawPath);
 
    if (!std::filesystem::exists(metadataPath))
    {
        auto writeResult = WriteMetadata(metadataPath, rawPath);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(writeResult)
    }
    
    return metadataPath;
}
}
