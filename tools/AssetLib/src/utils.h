#pragma once

#include "types.h"

#include <vector>

namespace utils
{
    u64 compressToBlob(std::vector<u8>& blob, const void* source, u64 sourceSizeBytes);
    u64 compressToBlob(std::vector<u8>& blob, const std::vector<const void*>& sources, const std::vector<u64>& sourceSizesBytes);
}