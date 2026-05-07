#include "MeshImporter.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetImportLib/Importers/Import.h>
#include <CoreLib/Utils/FileUtils.h>

namespace lux::import
{
MeshImporter::MeshImporter(const std::shared_ptr<Context>& ctx)
    : Importer(ctx)
{
}

ImportResult<void> MeshImporter::Import(const std::filesystem::path& path, ImportFlags importFlags)
{
    auto importPath = EnsureMetadata(path);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importPath)
    auto metadataRead = assetlib::sceneMesh::readMeta(*importPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    m_ImportedMeta = *metadataRead;
    
    auto meshRead = assetlib::sceneMesh::readMesh(m_ImportedMeta.Metadata);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(meshRead)
    m_ImportedAsset.Asset = *meshRead;
    
    return {};
}

ImportResult<assetlib::AssetId> MeshImporter::Export(const assetlib::MeshAsset& asset,
    const std::filesystem::path& exportPath)
{
    auto metadataPath = EnsureMetadata(exportPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataPath)
    auto metadataRead = assetlib::sceneMesh::readMeta(*metadataPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    
    auto packedMesh = assetlib::sceneMesh::pack(asset);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(packedMesh)
    
    IoResult<u64> saveResult = m_Ctx->Io->WriteHeader(metadataRead->Metadata, packedMesh->Header);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(saveResult)
    
    metadataRead->Metadata.Io.HeaderSizeBytes = *saveResult;
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(
        UpdatePackedMetadataSilent(*metadataPath, assetlib::sceneMesh::packMeta(*metadataRead), "mesh"))
    
    return metadataRead->Metadata.AssetId;
}

std::filesystem::path MeshImporter::GetMetaPath(const std::filesystem::path& path) const
{
    return assetlib::getMetadataPath(path);
}

IoResult<void> MeshImporter::WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath)
{
    const assetlib::MeshMeta meshMeta = {
        .Metadata = CreateMetadataBase(metaPath, rawPath, assetlib::sceneMesh::getTypeMetadata(),
            SCENE_MESH_ASSET_EXTENSION, *m_Ctx)
    };
    
    return WritePackedMetadata(metaPath, assetlib::sceneMesh::packMeta(meshMeta), "mesh");
}
}
