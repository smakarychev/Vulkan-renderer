#pragma once
#include "AssetIoInterface.h"

namespace lux::assetlib::io
{
class SeparateAssetIoInterface final : public AssetIoInterface
{
public:
    IoResult<void> WriteHeader(const AssetMetadata& metadata, const AssetCustomHeaderType& header) override;
    IoResult<u64> WriteBinaryChunk(const AssetMetadata& metadata, Span<const std::byte> binaryDataChunk) override;
    IoResult<AssetCustomHeaderType> ReadHeader(const AssetMetadata& metadata) override;
    IoResult<void> ReadBinaryChunk(const AssetMetadata& metadata, std::byte* destination, u64 offsetBytes,
        u64 sizeBytes) override;
    
    std::string GetHeaderExtension(std::string_view preferred) const override;
    std::string GetBinariesExtension() const override;
    
    DEFINE_ASSET_IO_INTERFACE_NAME("Separate", "a463a68f-5504-43d4-9d92-563105704240"_guid)
};
}
