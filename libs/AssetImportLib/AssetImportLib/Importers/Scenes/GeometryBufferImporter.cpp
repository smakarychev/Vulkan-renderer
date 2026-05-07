#include "GeometryBufferImporter.h"

#include "SceneImporter.h"

#include <AssetLib/Io/IoInterface/AssetIoInterface.h>
#include <AssetLib/Io/Compression/AssetCompressor.h>
#include <AssetImportLib/Importers/Import.h>
#include <AssetImportLib/Bakers/BakersUtils.h>
#include <CoreLib/Utils/HashFileUtils.h>
#include <CoreLib/Utils/FileUtils.h>

namespace lux::import
{
GeometryBufferImporter::GeometryBufferImporter(const std::shared_ptr<Context>& ctx, const MetadataHint& metadataHint)
    : Importer(ctx), m_MetadataHint(metadataHint)
{
}

ImportResult<void> GeometryBufferImporter::Import(const std::filesystem::path& path, ImportFlags importFlags)
{
    CHECK_RETURN_IMPORT_ERROR(enumHasAny(importFlags, ImportFlags::Binaries | ImportFlags::Header),
        IoError::ErrorCode::GeneralError, 
        "GeometryBufferImporter error flags do not include neither header nor binaries {}", path.string())
    
    auto importPath = EnsureMetadata(path);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importPath)
    auto metadataRead = assetlib::sceneGeometry::readMeta(*importPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    m_ImportedMeta = *metadataRead;
    
    if (enumHasAny(importFlags, ImportFlags::Header))
    {
        auto headerRead = assetlib::sceneGeometry::readHeader(m_ImportedMeta.Metadata);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(headerRead)
        m_ImportedAsset.Asset.Header = std::move(*headerRead);
    }
    if (enumHasAny(importFlags, ImportFlags::Binaries))
    {
        const auto& header = m_ImportedAsset.Asset.Header;
        auto binariesRead = assetlib::sceneGeometry::readBufferData(header, m_ImportedMeta.Metadata,
            *m_Ctx->Io, *m_Ctx->Compressor);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(binariesRead)
        m_ImportedAsset.Asset.Data = std::move(*binariesRead);
    }
    
    return {};
}

ImportResult<assetlib::AssetId> GeometryBufferImporter::Export(const assetlib::GeometryBufferAsset& asset,
    const std::filesystem::path& exportPath)
{
    auto metadataPath = EnsureMetadata(exportPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataPath)
    auto metadataRead = assetlib::sceneGeometry::readMeta(*metadataPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    
    auto packedGeometry = assetlib::sceneGeometry::pack(asset, *m_Ctx->Compressor);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(packedGeometry)
    
    IoResult<u64> saveResult = m_Ctx->Io->WriteHeader(metadataRead->Metadata, packedGeometry->Header);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(saveResult)
    
    IoResult<u64> binarySaveResult = m_Ctx->Io->WriteBinaryChunk(metadataRead->Metadata, 
        packedGeometry->PackedBinaries);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(binarySaveResult)
    
    metadataRead->Metadata.Io.HeaderSizeBytes = *saveResult;
    metadataRead->Metadata.Io.BinarySizeBytes = asset.Data.size();
    metadataRead->Metadata.Io.BinarySizeBytesCompressed = *binarySaveResult;
    metadataRead->Metadata.Io.BinarySizeBytesChunksCompressed = std::move(packedGeometry->PackedBinarySizeBytesChunks);
    metadataRead->Metadata.Io.IoMode = m_Ctx->Io->GetName();
    metadataRead->Metadata.Io.CompressionMode = m_Ctx->Compressor->GetName();
    metadataRead->Metadata.Io.IoGuid = m_Ctx->Io->GetGuid();
    metadataRead->Metadata.Io.CompressionGuid = m_Ctx->Compressor->GetGuid();
    metadataRead->Metadata.Io.CompressionGuid = m_Ctx->Compressor->GetGuid();
    metadataRead->SourceHash = m_MetadataHint.SourceHash;
    metadataRead->SourceUri = m_MetadataHint.SourceUri;
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(Importer::UpdatePackedMetadataSilent(
            *metadataPath, assetlib::sceneGeometry::packMeta(*metadataRead), "geometry"))
    
    return metadataRead->Metadata.AssetId;
}

bool GeometryBufferImporter::NeedsBaking(const std::filesystem::path& path) const
{
    auto metaRead = assetlib::sceneGeometry::readMeta(GetMetaPath(path));
    if (!metaRead.has_value())
        return true;
    
    return metaRead->SourceHash != SceneImporter::CalculateGeometryBufferHash(path, metaRead->SourceUri);
}

std::filesystem::path GeometryBufferImporter::GetMetaPath(const std::filesystem::path& path) const
{
    if (assetlib::isMetadataPath(path))
        return path;
    
    std::filesystem::path metaPath = path;
    metaPath += std::format(".{}", m_MetadataHint.SourceUri);
    
    return assetlib::getMetadataPath(path);
}

IoResult<void> GeometryBufferImporter::WriteMetadata(const std::filesystem::path& metaPath,
    const std::filesystem::path& rawPath)
{
    const assetlib::GeometryBufferMeta geometryBufferMeta = {
        .Metadata = CreateMetadataBase(metaPath, rawPath, assetlib::sceneGeometry::getTypeMetadata(),
            SCENE_GEOMETRY_BUFFER_ASSET_EXTENSION, *m_Ctx),
        .SourceHash = m_MetadataHint.SourceHash,
        .SourceUri = m_MetadataHint.SourceUri
    };
    
    return WritePackedMetadata(metaPath, assetlib::sceneGeometry::packMeta(geometryBufferMeta), "geometry");
}
}
