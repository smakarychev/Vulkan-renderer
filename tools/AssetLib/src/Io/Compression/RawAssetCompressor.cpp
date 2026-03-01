#include "RawAssetCompressor.h"

namespace lux::assetlib::io
{
std::vector<std::byte> RawAssetCompressor::Compress(Span<const std::byte> data)
{
    return {data.begin(), data.end()};
}

std::vector<std::byte> RawAssetCompressor::Decompress(Span<const std::byte> data, u64 decompressedSize)
{
    return {data.begin(), data.end()};
}
}
