#pragma once

#include "AssetLib/AssetId.h"
#include "AssetLib/Io/AssetIo.h"

#include <CoreLib/Containers/Guid.h>

namespace lux::assetlib
{
struct AssetSystemIdentity
{
    std::string Name{};
    Guid Guid{};
};

struct AssetPackageMetadata2
{
    struct ImporterInfo
    {
        AssetSystemIdentity ImporterType{};
        std::string Info{};
    };
    struct AssetMeta
    {
        std::optional<std::string> VariantName{};
        AssetId AssetId{};
        AssetSystemIdentity AssetTypeInfo{};
        std::chrono::system_clock::time_point LastDerivationTime{};
        ImporterInfo ImporterInfo{};
    };
    
    u32 Version{0};
    AssetId PackageId{};
    AssetSystemIdentity Locator{};
    std::vector<AssetMeta> Assets{};
    std::string OriginalFile{};
    
    static io::IoResult<AssetPackageMetadata2> FromString(const std::string& buffer);
    std::string ToString() const;
    std::string ToFormattedString() const;
};

}
