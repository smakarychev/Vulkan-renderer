#pragma once

#include "types.h"

namespace utils
{
    static constexpr auto FNV_OFFSET_BASIS = 0xcbf29ce484222325ull;
    static constexpr auto FNV_PRIME = 0x100000001b3ull;
    
    static constexpr auto FNV_OFFSET_BASIS_32 = 0x811c9dc5;
    static constexpr auto FNV_PRIME_32 = 0x01000193;

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
    
    inline u32 hashBytes32(const void* data, u32 sizeBytes, u32 offsetBasis = FNV_OFFSET_BASIS_32)
    {
        u32 hash = offsetBasis;
        for (const u8* byte = static_cast<const u8*>(data); byte < static_cast<const u8*>(data) + sizeBytes; byte++)
        {
            hash = hash ^ static_cast<u32>(*byte);
            hash = hash * FNV_PRIME_32;
        }
        
        return hash;
    }
    
    constexpr u64 hashStringBytes(const char* data, u32 sizeBytes, u64 offsetBasis = FNV_OFFSET_BASIS)
    {
        if (std::is_constant_evaluated())
        {
            return sizeBytes == 0 ?
               offsetBasis :
               hashStringBytes(data + 1, sizeBytes - 1, (offsetBasis ^ (u64)*data) * FNV_PRIME);
        }
        
        return hashBytes(data, sizeBytes);
    }
    
    constexpr u32 hashStringBytes32(const char* data, u32 sizeBytes, u32 offsetBasis = FNV_OFFSET_BASIS_32)
    {
        if (std::is_constant_evaluated())
        {
            return sizeBytes == 0 ?
               offsetBasis :
               hashStringBytes32(data + 1, sizeBytes - 1, (offsetBasis ^ (u32)*data) * FNV_PRIME_32);
        }
        
        return hashBytes32(data, sizeBytes);
    }

    template<u64 N>
    constexpr u64 hashString(const char (&s)[N])
    {
        return hashStringBytes(s, N - 1);
    }
    template<u64 N>
    constexpr u32 hashString32(const char (&s)[N])
    {
        return hashStringBytes32(s, N - 1);
    }

    constexpr u64 hashString(std::string_view string)
    {
        return hashStringBytes(string.data(), (u32)string.size());
    }
    
    constexpr u64 hashString32(std::string_view string)
    {
        return hashStringBytes32(string.data(), (u32)string.size());
    }
}

