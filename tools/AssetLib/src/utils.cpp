#include "utils.h"

#include <numeric>

#include "lz4.h"

namespace utils
{
    u64 compressToBlob(std::vector<u8>& blob, const void* source, u64 sourceSizeBytes)
    {
        u32 compressedSizeBound = LZ4_compressBound((i32)sourceSizeBytes);
        blob.resize(compressedSizeBound);
        u32 compressedSize = LZ4_compress_default((const char*)source, (char*)blob.data(), (i32)sourceSizeBytes,
            (i32)compressedSizeBound);
        blob.resize(compressedSize);

        return (u64)compressedSize;
    }

    u64 compressToBlob(std::vector<u8>& blob, const std::vector<const void*>& sources,
        const std::vector<u64>& sourceSizesBytes)
    {
        u64 totalSize = std::accumulate(sourceSizesBytes.begin(), sourceSizesBytes.end(), 0llu);
        std::vector<u8> accumulated(totalSize);
        u64 offset = 0;
        for (u32 i = 0; i < sources.size(); i++)
        {
            memcpy(accumulated.data() + offset, sources[i], sourceSizesBytes[i]);
            offset += sourceSizesBytes[i];
        }
        return compressToBlob(blob, accumulated.data(), accumulated.size());
    }
}


