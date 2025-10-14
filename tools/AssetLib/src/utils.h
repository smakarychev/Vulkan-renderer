#pragma once

#include "types.h"
#include "Containers/Span.h"
#include "v2/AssetLibV2.h"

#include <vector>

namespace assetlib
{
struct AssetFile;
}

namespace assetlib::utils
{
// todo: delete this once assets are fully transitioned
u64 packLz4(std::vector<u8>& destination, const void* source, u64 sourceSizeBytes);

std::vector<std::byte> pack(Span<const std::byte> source, CompressionMode compressionMode);
std::vector<std::byte> packLz4(Span<const std::byte> source);
std::vector<std::byte> packRaw(Span<const std::byte> source);

std::vector<std::byte> unpack(Span<const std::byte> source, u64 unpackedSize, CompressionMode compressionMode);
std::vector<std::byte> unpackLz4(Span<const std::byte> source, u64 unpackedSize);
std::vector<std::byte> unpackRaw(Span<const std::byte> source);
}