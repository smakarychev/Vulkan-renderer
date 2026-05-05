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

    static assetlib::AssetMetadata CreateMetadataBase(const std::filesystem::path& metaPath,
        const std::filesystem::path& rawPath, const assetlib::AssetTypeMetadata& typeMetadata);
    static IoResult<void> WritePackedMetadata(const std::filesystem::path& metaPath,
        const assetlib::io::IoResult<std::string>& metadata, std::string_view typeLabel);
protected:
    std::shared_ptr<Context> m_Ctx;
};
}

CREATE_ENUM_FLAGS_OPERATORS(lux::import::ImportFlags)

#define CHECK_RETURN_IMPORT_ERROR(x, error, ...) \
if (!(x)) { return std::unexpected<ImportError>(::lux::assetlib::io::IoError{.Code = error, .Message = std::format(__VA_ARGS__)}); }

#define CHECK_RETURN_IMPORT_ERROR_PROPAGATE(result) \
if (!(result).has_value()) { return std::unexpected<ImportError>((result).error()); }
