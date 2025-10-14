﻿#include "utils.h"

#include "core.h"

#include <lz4.h>

namespace assetlib::utils
{
u64 packLz4(std::vector<u8>& destination, const void* source, u64 sourceSizeBytes)
{
    const u32 compressedSizeBound = LZ4_compressBound((i32)sourceSizeBytes);
    destination.resize(compressedSizeBound);
    const u32 compressedSize = LZ4_compress_default((const char*)source, (char*)destination.data(), (i32)sourceSizeBytes,
        (i32)compressedSizeBound);
    destination.resize(compressedSize);

    return compressedSize;
}

std::vector<std::byte> pack(Span<const std::byte> source, CompressionMode compressionMode)
{
    switch (compressionMode)
    {
    case CompressionMode::Raw:
        return packRaw(source);
    case CompressionMode::LZ4:
        return packLz4(source);
    default:
        ASSERT(false, "Unexpected compression mode: {}", (u32)compressionMode)
        return packRaw(source);
    }
}

std::vector<std::byte> packLz4(Span<const std::byte> source)
{
    const u32 compressedSizeBound = LZ4_compressBound((i32)source.size());
    std::vector<std::byte> destination(compressedSizeBound);
    const u32 compressedSize = LZ4_compress_default((const char*)source.data(), (char*)destination.data(),
        (i32)source.size(), (i32)compressedSizeBound);
    destination.resize(compressedSize);

    return destination;
}

std::vector<std::byte> packRaw(Span<const std::byte> source)
{
    return std::vector(source.begin(), source.end());
}

std::vector<std::byte> unpack(Span<const std::byte> source, u64 unpackedSize, CompressionMode compressionMode)
{
    switch (compressionMode)
    {
    case CompressionMode::Raw:
        return unpackRaw(source);
    case CompressionMode::LZ4:
        return unpackLz4(source, unpackedSize);
    default:
        ASSERT(false, "Unexpected compression mode: {}", (u32)compressionMode)
        return unpackRaw(source);
    }
}

std::vector<std::byte> unpackLz4(Span<const std::byte> source, u64 unpackedSize)
{
    if (unpackedSize == source.size())
        return unpackRaw(source);
    
    std::vector<std::byte> unpacked(unpackedSize);
    LZ4_decompress_safe((const char*)source.data(), (char*)unpacked.data(), (i32)source.size(), (i32)unpackedSize);

    return unpacked;
}

std::vector<std::byte> unpackRaw(Span<const std::byte> source)
{
    return std::vector(source.begin(), source.end());
}
}
