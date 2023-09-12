#pragma once

#include "types.h"

#include <vector>

namespace utils
{
    void compressToBlob(std::vector<u8>& blob, const void* source, u64 sourceSizeBytes);
    void compressToBlob(std::vector<u8>& blob, const std::vector<const void*>& sources, const std::vector<u64>& sourceSizesBytes);
}