#pragma once

#include "types.h"

#include <cstddef>
#include <cstring>

namespace Hash
{
    inline u32 murmur3b32(const u8* data, usize sizeBytes, u32 seed = 0)
    {
        auto scramble = [](u32 k)
        {
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            return k;
        };
        
        u32 h = seed;
        u32 k;
        for (usize i = sizeBytes >> 2; i; i--) {
            memcpy(&k, data, sizeof(u32));
            data += sizeof(u32);
            h ^= scramble(k);
            h = (h << 13) | (h >> 19);
            h = h * 5 + 0xe6546b64;
        }
        k = 0;
        for (usize i = sizeBytes & 3; i; i--) {
            k <<= 8;
            k |= data[i - 1];
        }
        h ^= scramble(k);
        h ^= sizeBytes;
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        
        return h;
    }
}
