#include "SceneImporter.h"

#include <CoreLib/utils/HashFileUtils.h>
#include <CoreLib/utils/FileUtils.h>

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
    
    auto importPath = EnsureMetadata(path);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(importPath)
    auto metadataRead = assetlib::scene::readMeta(*importPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    m_ImportedMeta = *metadataRead;
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(EnsureBaked(*importPath, importFlags, m_ImportedMeta, m_Baker, "scene"))
    
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
