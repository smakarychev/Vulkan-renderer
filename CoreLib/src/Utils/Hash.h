#pragma once

#include "types.h"

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

    static constexpr auto FNV_OFFSET_BASIS = 0xcbf29ce484222325ull;
    static constexpr auto FNV_PRIME = 0x100000001b3ull;
    
    static constexpr auto FNV_OFFSET_BASIS_32 = 0x811c9dc5;
    static constexpr auto FNV_PRIME_32 = 0x01000193;

    /* taken from boost > 1.51 */
    constexpr void combine(u64& h, u64 k)
    {
        constexpr u64 m = 0xc6a4a7935bd1e995uLL;
        constexpr i32 r = 47;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;

        h += 0xe6546b64;
    }

    inline u64 bytes(const void* data, u32 sizeBytes, u64 offsetBasis = FNV_OFFSET_BASIS)
    {
        u64 hash = offsetBasis;
        for (const u8* byte = static_cast<const u8*>(data); byte < static_cast<const u8*>(data) + sizeBytes; byte++)
        {
            hash = hash ^ static_cast<u64>(*byte);
            hash = hash * FNV_PRIME;
        }
        
        return hash;
    }
    
    inline u32 bytes32(const void* data, u32 sizeBytes, u32 offsetBasis = FNV_OFFSET_BASIS_32)
    {
        u32 hash = offsetBasis;
        for (const u8* byte = static_cast<const u8*>(data); byte < static_cast<const u8*>(data) + sizeBytes; byte++)
        {
            hash = hash ^ static_cast<u32>(*byte);
            hash = hash * FNV_PRIME_32;
        }
        
        return hash;
    }
    
    constexpr u64 stringBytes(const char* data, u32 sizeBytes, u64 offsetBasis = FNV_OFFSET_BASIS)
    {
        if (std::is_constant_evaluated())
        {
            return sizeBytes == 0 ?
               offsetBasis :
               stringBytes(data + 1, sizeBytes - 1, (offsetBasis ^ (u64)*data) * FNV_PRIME);
        }
        
        return bytes(data, sizeBytes);
    }
    
    constexpr u32 stringBytes32(const char* data, u32 sizeBytes, u32 offsetBasis = FNV_OFFSET_BASIS_32)
    {
        if (std::is_constant_evaluated())
        {
            return sizeBytes == 0 ?
               offsetBasis :
               stringBytes32(data + 1, sizeBytes - 1, (offsetBasis ^ (u32)*data) * FNV_PRIME_32);
        }
        
        return bytes32(data, sizeBytes);
    }

    template<u64 N>
    constexpr u64 string(const char (&s)[N])
    {
        return stringBytes(s, N - 1);
    }
    template<u64 N>
    constexpr u32 string32(const char (&s)[N])
    {
        return stringBytes32(s, N - 1);
    }

    constexpr u64 string(std::string_view string)
    {
        return stringBytes(string.data(), (u32)string.size());
    }
    
    constexpr u64 string32(std::string_view string)
    {
        return stringBytes32(string.data(), (u32)string.size());
    }
}
