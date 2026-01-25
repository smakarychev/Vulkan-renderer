#pragma once
#include "AssetIoInterface.h"

namespace lux::assetlib::io
{
class CombinedAssetIoInterface final : public AssetIoInterface
{
public:
    IoResult<void> WriteHeader(const AssetFile& file) override;
    IoResult<u64> WriteBinaryChunk(const AssetFile& file, Span<const std::byte> binaryDataChunk) override;
    IoResult<AssetFile> ReadHeader(const std::filesystem::path& headerPath) override;
    IoResult<void> ReadBinaryChunk(
        const AssetFile& file, std::byte* destination, u64 offsetBytes, u64 sizeBytes) override;
    
    std::string GetHeaderExtension(std::string_view preferred) const override;
    std::string GetBinariesExtension() const override;

    DEFINE_ASSET_IO_INTERFACE_NAME("Combined", "6ca189c5-4e11-4bd1-98a9-7749d3cd3f55"_guid)
};
}
