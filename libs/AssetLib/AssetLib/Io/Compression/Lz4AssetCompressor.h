#pragma once
#include "AssetCompressor.h"

namespace lux::assetlib::io
{
class Lz4AssetCompressor final : public AssetCompressor
{
public:
    std::vector<std::byte> Compress(Span<const std::byte> data) override;
    std::vector<std::byte> Decompress(Span<const std::byte> data, u64 decompressedSize) override;

    DEFINE_ASSET_COMPRESSOR_NAME("Lz4", "1dc6c11f-8401-4d3a-9f04-0bfff98d0e6f"_guid)
};
}
