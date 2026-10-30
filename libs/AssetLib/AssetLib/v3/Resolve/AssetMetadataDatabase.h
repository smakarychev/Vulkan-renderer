#pragma once

#include "AssetLib/v3/Assetlibv3.h"

namespace lux::assetlib
{
class IAssetMetadataDatabase : public IAssetSystemIdentifiable
{
public:
    LUX_RTTI(IAssetMetadataDatabase, IAssetSystemIdentifiable)

    virtual std::optional<AssetPackageMetadata2> Find(const AssetResolvedPath& resolvedPath) const = 0;
    virtual std::optional<AssetPackageMetadata2> Find(const AssetId& packageId) const = 0;
};
}
