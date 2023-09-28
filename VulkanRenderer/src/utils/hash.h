#pragma once
#include "types.h"

namespace utils
{
    static constexpr auto FNV_OFFSET_BASIS = 0xcbf29ce484222325ull;
    static constexpr auto FNV_PRIME = 0x100000001b3ull;
    
    inline u64 hashBytes(const void* data, u32 sizeBytes, u64 offsetBasis = FNV_OFFSET_BASIS)
    {
        u64 hash = offsetBasis;
        for (const u8* byte = static_cast<const u8*>(data); byte < static_cast<const u8*>(data) + sizeBytes; byte++)
        {
            hash = hash ^ static_cast<u64>(*byte);
            hash = hash * FNV_PRIME;
        }
        return hash;
    }    
}

