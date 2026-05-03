#include "MaterialImporter.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <CoreLib/Utils/FileUtils.h>

#define CHECK_RETURN_IO_ERROR(x, error, ...) \
ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, __VA_ARGS__)

namespace lux::import
{
MaterialImporter::MaterialImporter(const std::shared_ptr<Context>& ctx)
    : Importer(ctx)
{
}

ImportResult<void> MaterialImporter::Import(const std::filesystem::path& path, ImportFlags importFlags)
{
    const std::filesystem::path importPath = GetMetaPath(path);
    
    if (!std::filesystem::exists(importPath))
    {
        auto writeResult = WriteMetadata(importPath, path);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(writeResult)
    }

    auto metadataRead = assetlib::material::readMeta(importPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    m_ImportedMeta = *metadataRead;
    
    auto materialRead = assetlib::material::readMaterial(m_ImportedMeta.Metadata);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(materialRead)
    m_ImportedAsset.Asset = *materialRead;
    
    return {};
}

ImportResult<assetlib::AssetId> MaterialImporter::Export(const assetlib::MaterialAsset& asset,
    const std::filesystem::path& exportPath)
{
    std::filesystem::path exportMetadataPath = GetMetaPath(exportPath);
    
    if (!std::filesystem::exists(exportMetadataPath))
    {
        auto writeResult = WriteMetadata(exportMetadataPath, exportPath);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(writeResult)
    }
    
    auto metadataRead = assetlib::material::readMeta(exportMetadataPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    
    auto packedMaterial = assetlib::material::pack(asset);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(packedMaterial)
    
    IoResult<void> saveResult = m_Ctx->Io->WriteHeader(metadataRead->Metadata, packedMaterial->Header);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(saveResult)
    
    return metadataRead->Metadata.AssetId;
}

std::filesystem::path MaterialImporter::GetMetaPath(const std::filesystem::path& path) const
{
    return assetlib::getMetadataPath(path);
}

IoResult<void> MaterialImporter::WriteMetadata(const std::filesystem::path& metaPath, 
    const std::filesystem::path& rawPath)
{
    if (std::filesystem::exists(metaPath))
        return {};
    
    const assetlib::AssetId assetId = assetlib::material::readMeta(metaPath).value_or({}).Metadata.AssetId;
    
    assetlib::MaterialMeta materialMeta = {
        .Metadata = {
            .AssetId = assetId,
            .Type = assetlib::material::getTypeMetadata(),
            .Io = {
                .OriginalFile = std::filesystem::weakly_canonical(rawPath).generic_string(),
                .HeaderFile = std::filesystem::weakly_canonical(rawPath).generic_string(),
                .BinaryFile = std::filesystem::weakly_canonical(rawPath).generic_string(),
            }
        }
    };
    
    auto packed = assetlib::material::packMeta(materialMeta);
    CHECK_RETURN_IO_ERROR(packed.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to pack material meta data for {}", rawPath.generic_string())

    auto writeResult = writeStringToFile(metaPath, assetlib::io::getAssetHeaderFormatted(*packed));
    CHECK_RETURN_IO_ERROR(writeResult.has_value(), IoError::ErrorCode::FailedToCreate,
        "Failed to create material meta data for {}", rawPath.generic_string())
    
    return {};
}
}
