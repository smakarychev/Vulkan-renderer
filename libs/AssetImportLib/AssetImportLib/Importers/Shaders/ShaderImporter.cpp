#include "ShaderImporter.h"

#include <CoreLib/Utils/FileUtils.h>

#define CHECK_RETURN_IO_ERROR(x, error, ...) \
ASSETLIB_CHECK_RETURN_IO_ERROR(x, error, __VA_ARGS__)


namespace lux::import
{
ShaderImporter::ShaderImporter(const std::shared_ptr<Context>& ctx, const ShaderImportSettings& settings)
    : Importer(ctx), m_ImportSettings(settings), m_Baker(ctx, m_ImportSettings) 
{
}

ImportResult<void> ShaderImporter::Import(const std::filesystem::path& path, ImportFlags importFlags)
{
    CHECK_RETURN_IMPORT_ERROR(enumHasAny(importFlags, ImportFlags::Binaries | ImportFlags::Header),
        IoError::ErrorCode::GeneralError, "ShaderImporter error flags do not include neither header nor binaries {}",
        path.string())
    
    const std::filesystem::path importPath = GetMetaPath(path);
    
    if (!std::filesystem::exists(importPath))
    {
        auto writeResult = WriteMetadata(importPath, path);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(writeResult)
    }
    
    auto metadataRead = assetlib::shader::readMeta(importPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(metadataRead)
    m_ImportedMeta = *metadataRead;
    
    auto shaderLoadRead = assetlib::shader::readLoadInfo(m_ImportedMeta.Metadata.Io.OriginalFile);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(shaderLoadRead)
    m_ImportedLoadInfo = *shaderLoadRead;
    
    if (m_Baker.ShouldBake(importPath))
    {
        if (!enumHasAny(importFlags, ImportFlags::BakeIfNotBaked))
            return std::unexpected(ImportError{
                {
                    .Code = IoError::ErrorCode::GeneralError,
                    .Message = std::format("Failed to import shader: {}", path.string())
                },
                ImportError::ImportErrorCode::NotBaked
            });
        
        auto bakeResult = m_Baker.BakeToFile(m_ImportedMeta, importPath);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(bakeResult)
    }
    
    if (enumHasAny(importFlags, ImportFlags::Header))
    {
        auto headerRead = assetlib::shader::readHeader(m_ImportedMeta.Metadata);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(headerRead)
        m_ImportedAsset.Asset.Header = std::move(*headerRead);
    }
    if (enumHasAny(importFlags, ImportFlags::Binaries))
    {
        const auto& header = m_ImportedAsset.Asset.Header;
        
        auto binariesRead = assetlib::shader::readSpirv(header, m_ImportedMeta.Metadata,
            *m_Ctx->Io, *m_Ctx->Compressor);
        CHECK_RETURN_IMPORT_ERROR_PROPAGATE(binariesRead)
        m_ImportedAsset.Asset.Spirv = std::move(*binariesRead);
    }
        
    return {};
}

bool ShaderImporter::NeedsBaking(const std::filesystem::path& path) const
{
    return m_Baker.ShouldBake(GetMetaPath(path));
}

std::filesystem::path ShaderImporter::GetMetaPath(const std::filesystem::path& path) const
{
    if (!assetlib::isMetadataPath(path))
    {
        auto loadInfoRead = assetlib::shader::readLoadInfo(path);
        if (!loadInfoRead)
            return {};
        return assetlib::getMetadataPath(
            m_Baker.GetDefineAwarePath(path, m_Baker.GetDefinesHash(*loadInfoRead).value_or(0)));
    }
    
    return path;
}

IoResult<void> ShaderImporter::WriteMetadata(const std::filesystem::path& metaPath,
    const std::filesystem::path& rawPath)
{
    if (std::filesystem::exists(metaPath))
        return {};
    
    const assetlib::AssetId assetId = assetlib::shader::readMeta(metaPath).value_or({}).Metadata.AssetId;
    
    assetlib::ShaderMeta shaderMeta = {
        .Metadata = {
            .AssetId = assetId,
            .Type = assetlib::shader::getTypeMetadata(),
            .Io = {
                .OriginalFile = std::filesystem::weakly_canonical(rawPath).generic_string(),
            }
        },
        .VariantName = m_ImportSettings.Variant.AsString()
    };
    
    auto packed = assetlib::shader::packMeta(shaderMeta);
    CHECK_RETURN_IO_ERROR(packed.has_value(), IoError::ErrorCode::GeneralError,
        "Failed to pack shader meta data for {}", rawPath.generic_string())

    auto writeResult = writeStringToFile(metaPath, assetlib::io::getAssetHeaderFormatted(*packed));
    CHECK_RETURN_IO_ERROR(writeResult.has_value(), IoError::ErrorCode::FailedToCreate,
        "Failed to create shader meta data for {}", rawPath.generic_string())

    return {};
}
}
