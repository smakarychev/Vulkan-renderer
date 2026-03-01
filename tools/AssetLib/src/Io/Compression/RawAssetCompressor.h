#pragma once
#include "AssetCompressor.h"

namespace lux::assetlib::io
{
class RawAssetCompressor final : public AssetCompressor
{
public:
    std::vector<std::byte> Compress(Span<const std::byte> data) override;
    std::vector<std::byte> Decompress(Span<const std::byte> data, u64 decompressedSize) override;

    DEFINE_ASSET_COMPRESSOR_NAME("Raw", "984c9972-a897-4d0b-b276-92b3331c0d1f"_guid)
};
}
