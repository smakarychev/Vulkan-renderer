#include "MaterialImporter.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetImportLib/Importers/Import.h>
#include <CoreLib/Utils/FileUtils.h>

namespace lux::import
{
MaterialImporter::MaterialImporter(const std::shared_ptr<Context>& ctx)
    : Importer(ctx)
{
}

ImportResult<void> MaterialImporter::Import(const std::filesystem::path& path, ImportFlags importFlags)
{
    auto importPath = EnsureMetadata(path);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importPath)
    auto metadataRead = assetlib::material::readMeta(*importPath);
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
    auto metadataPath = EnsureMetadata(exportPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataPath)
    auto metadataRead = assetlib::material::readMeta(*metadataPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    
    auto packedMaterial = assetlib::material::pack(asset);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(packedMaterial)
    
    IoResult<u64> saveResult = m_Ctx->Io->WriteHeader(metadataRead->Metadata, packedMaterial->Header);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(saveResult)
    
    metadataRead->Metadata.Io.HeaderSizeBytes = *saveResult;
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(
        UpdatePackedMetadataSilent(*metadataPath, assetlib::material::packMeta(*metadataRead), "material"))
    
    return metadataRead->Metadata.AssetId;
}

std::filesystem::path MaterialImporter::GetMetaPath(const std::filesystem::path& path) const
{
    return assetlib::getMetadataPath(path);
}

IoResult<void> MaterialImporter::WriteMetadata(const std::filesystem::path& metaPath, 
    const std::filesystem::path& rawPath)
{
    assetlib::MaterialMeta materialMeta = {
        .Metadata = CreateMetadataBase(metaPath, rawPath, assetlib::material::getTypeMetadata(),
            MATERIAL_ASSET_EXTENSION, *m_Ctx)
    };
    materialMeta.Metadata.Io.HeaderFile = materialMeta.Metadata.Io.BinaryFile = materialMeta.Metadata.Io.OriginalFile;
    
    return WritePackedMetadata(metaPath, assetlib::material::packMeta(materialMeta), "material");
}
}
