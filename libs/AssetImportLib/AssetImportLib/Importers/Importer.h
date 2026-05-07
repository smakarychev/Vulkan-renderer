#pragma once

#include "ImportResult.h"

namespace lux::import
{
enum class ImportFlags : u8
{
    None = 0,
    BakeIfNotBaked = BIT(0),
    Header = BIT(1),
    Binaries = BIT(2),
};

struct ImportedAsset{};

class Importer
{
public:
    Importer(const std::shared_ptr<Context>& ctx) : m_Ctx(ctx) {}
    virtual ~Importer() = default;
    virtual ImportResult<void> Import(const std::filesystem::path& path, ImportFlags importFlags) = 0;
    ImportResult<void> Import(const std::filesystem::path& path);
    virtual const ImportedAsset& GetImportedAsset() const = 0;
    virtual const assetlib::AssetMetadata& GetImportedAssetMetadata() const = 0;
    
    virtual bool NeedsBaking(const std::filesystem::path& path) const = 0;
    virtual std::filesystem::path GetMetaPath(const std::filesystem::path& path) const = 0;
    
    /* updates metadata file, without changing the file modification time */
    static IoResult<void> UpdatePackedMetadataSilent(const std::filesystem::path& metaPath,
        const assetlib::io::IoResult<std::string>& metadata, std::string_view typeLabel);
protected:
    static assetlib::AssetMetadata CreateMetadataBase(const std::filesystem::path& metaPath,
        const std::filesystem::path& rawPath, const assetlib::AssetTypeMetadata& typeMetadata,
        std::string_view postBakeExtension, const Context& ctx);
    static IoResult<void> WritePackedMetadata(const std::filesystem::path& metaPath,
        const assetlib::io::IoResult<std::string>& metadata, std::string_view typeLabel);
    
    ImportResult<std::filesystem::path> EnsureMetadata(const std::filesystem::path& rawPath);
    template <typename Metadata, typename Baker>
    static ImportResult<void> EnsureBaked(const std::filesystem::path& metaPath, ImportFlags importFlags,
        Metadata& metadata, Baker& baker, std::string_view typeLabel);
    virtual IoResult<void> WriteMetadata(const std::filesystem::path& metaPath, 
        const std::filesystem::path& rawPath) = 0;
protected:
    std::shared_ptr<Context> m_Ctx;
};

}

CREATE_ENUM_FLAGS_OPERATORS(lux::import::ImportFlags)

#define CHECK_RETURN_IMPORT_ERROR(x, error, ...) \
if (!(x)) { return std::unexpected<ImportError>(::lux::assetlib::io::IoError{.Code = error, .Message = std::format(__VA_ARGS__)}); }

#define CHECK_RETURN_IMPORT_ERROR_PROPAGATE(result) \
if (!(result).has_value()) { return std::unexpected<ImportError>((result).error()); }

namespace lux::import
{
template <typename Metadata, typename Baker>
ImportResult<void> Importer::EnsureBaked(const std::filesystem::path& metaPath, ImportFlags importFlags, 
    Metadata& metadata, Baker& baker, std::string_view typeLabel)
{
    if (!baker.NeedsBaking(metaPath))
        return {};
    if (!enumHasAny(importFlags, ImportFlags::BakeIfNotBaked))
        return std::unexpected(ImportError{
            {
                .Code = IoError::ErrorCode::GeneralError,
                .Message = std::format("Failed to import {}: {}", typeLabel, metaPath.string())
            },
            ImportError::ImportErrorCode::NotBaked
        });
        
    auto bakeResult = baker.BakeToFile(metadata, metaPath);
    CHECK_RETURN_IMPORT_ERROR_PROPAGATE(bakeResult)
    
    return {};
}
}