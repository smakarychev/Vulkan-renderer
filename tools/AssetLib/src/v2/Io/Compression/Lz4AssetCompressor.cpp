#include "Lz4AssetCompressor.h"

#include <lz4.h>

namespace lux::assetlib::io
{
std::vector<std::byte> Lz4AssetCompressor::Compress(Span<const std::byte> data)
{
    const u32 compressedSizeBound = LZ4_compressBound((i32)data.size());
    std::vector<std::byte> compressed(compressedSizeBound);
    const u32 compressedSize = LZ4_compress_default((const char*)data.data(), (char*)compressed.data(),
        (i32)data.size(), (i32)compressedSizeBound);
    compressed.resize(compressedSize);

    return compressed;
}

std::vector<std::byte> Lz4AssetCompressor::Decompress(Span<const std::byte> data, u64 decompressedSize)
{
    std::vector<std::byte> unpacked(decompressedSize);
    LZ4_decompress_safe((const char*)data.data(), (char*)unpacked.data(), (i32)data.size(), (i32)decompressedSize);

    return unpacked;
}
}

