#pragma once

#include "types.h"
#include "Containers/Span.h"
#include "v2/AssetLibV2.h"

#include <vector>

namespace lux::assetlib
{
struct AssetFile;
}

namespace lux::assetlib::utils
{
// todo: delete this once assets are fully transitioned
u64 packLz4(std::vector<u8>& destination, const void* source, u64 sourceSizeBytes);

std::vector<std::byte> packLz4(Span<const std::byte> source);

std::vector<std::byte> unpackLz4(Span<const std::byte> source, u64 unpackedSize);
}