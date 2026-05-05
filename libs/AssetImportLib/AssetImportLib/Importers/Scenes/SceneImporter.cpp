#include "SceneImporter.h"

#include <CoreLib/Utils/FileUtils.h>

#define CHECK_RETURN_IO_ERROR(x, error, ...) \
ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, __VA_ARGS__)

namespace lux::import
{
SceneImporter::SceneImporter(const std::shared_ptr<Context>& ctx)
    : Importer(ctx), m_Baker(ctx)
{
}

ImportResult<void> SceneImporter::Import(const std::filesystem::path& path, ImportFlags importFlags)
{
    CHECK_RETURN_IMPORT_ERROR(enumHasAny(importFlags, ImportFlags::Binaries | ImportFlags::Header),
        IoError::ErrorCode::GeneralError, "SceneImporter error flags do not include neither header nor binaries {}",
        path.string())
    
    const std::filesystem::path importPath = GetMetaPath(path);
 
    if (!std::filesystem::exists(importPath))
    {
        auto writeResult = WriteMetadata(importPath, path);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(writeResult)
    }
    
    auto metadataRead = assetlib::scene::readMeta(importPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    m_ImportedMeta = *metadataRead;
    
    if (m_Baker.ShouldBake(importPath))
    {
        if (!enumHasAny(importFlags, ImportFlags::BakeIfNotBaked))
            return std::unexpected(ImportError{
                {
                    .Code = IoError::ErrorCode::GeneralError,
                    .Message = std::format("Failed to import scene: {}", path.string())
                },
                ImportError::ImportErrorCode::NotBaked
            });
        
        auto bakeResult = m_Baker.BakeToFile(m_ImportedMeta, importPath);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(bakeResult)
    }
    
    if (enumHasAny(importFlags, ImportFlags::Header))
    {
        auto headerRead = assetlib::scene::readHeader(m_ImportedMeta.Metadata);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(headerRead)
        m_ImportedAsset.Asset.Header = std::move(*headerRead);
    }
    if (enumHasAny(importFlags, ImportFlags::Binaries))
    {
        const auto& header = m_ImportedAsset.Asset.Header;
        m_ImportedAsset.Asset.BuffersData.resize(header.Buffers.size());

        for (u32 buffer = 0; buffer < m_ImportedAsset.Asset.BuffersData.size(); buffer++)
        {
            auto binariesRead = assetlib::scene::readBufferData(header, m_ImportedMeta.Metadata, buffer,
                *m_Ctx->Io, *m_Ctx->Compressor);
            CHECK_RETURN_IMPORT_ERROR_PROPAGATE(binariesRead)
            m_ImportedAsset.Asset.BuffersData[buffer] = std::move(*binariesRead);
        }
    }
    
    return {};
}

bool SceneImporter::NeedsBaking(const std::filesystem::path& path) const
{
    return m_Baker.ShouldBake(GetMetaPath(path));
}

std::filesystem::path SceneImporter::GetMetaPath(const std::filesystem::path& path) const
{
    return assetlib::getMetadataPath(path);
}

{
    
    
IoResult<void> SceneImporter::WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath)
{
    const assetlib::SceneMeta sceneMeta = {
        .Metadata = CreateMetadataBase(metaPath, rawPath, assetlib::scene::getTypeMetadata())
    };
    
    return WritePackedMetadata(metaPath, assetlib::scene::packMeta(sceneMeta), "scene");
}
}
