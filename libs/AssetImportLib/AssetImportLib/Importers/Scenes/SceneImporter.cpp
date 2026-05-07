#include "SceneImporter.h"

#include <AssetImportLib/Importers/Import.h>
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
        auto headerRead = assetlib::scene::readScene(m_ImportedMeta.Metadata);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(headerRead)
        m_ImportedAsset.Asset = std::move(*headerRead);
    }
    
    return {};
}

bool SceneImporter::NeedsBaking(const std::filesystem::path& path) const
{
    return m_Baker.NeedsBaking(GetMetaPath(path));
}

std::filesystem::path SceneImporter::GetMetaPath(const std::filesystem::path& path) const
{
    return assetlib::getMetadataPath(path);
}

std::optional<u32> SceneImporter::CalculateGeometryBufferHash(const std::filesystem::path& path,
    const std::string& uri)
{
    return Hash::murmur3b32File(path.parent_path() / uri);
}

IoResult<void> SceneImporter::WriteMetadata(const std::filesystem::path& metaPath, const std::filesystem::path& rawPath)
{
    const assetlib::SceneMeta sceneMeta = {
        .Metadata = CreateMetadataBase(metaPath, rawPath, assetlib::scene::getTypeMetadata(),
            SCENE_ASSET_EXTENSION, *m_Ctx)
    };
    
    return WritePackedMetadata(metaPath, assetlib::scene::packMeta(sceneMeta), "scene");
}
}
