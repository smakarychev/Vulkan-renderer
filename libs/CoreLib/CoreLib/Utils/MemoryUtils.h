#pragma once

#include <CoreLib/types.h>

namespace lux::mem
{
constexpr u64 alignAddressPow2(u64 address, u16 alignment)
{
    const u16 mask = alignment - 1;
    
    return (address + mask) & ~mask;
}
constexpr u64 alignAddress(u64 address, u16 alignment)
{
    if consteval
    {
        if ((alignment & (alignment - 1)) == 0)
            return alignAddressPow2(address, alignment);
    }
    
    const u16 remainder = address % alignment;
    
    return address + (alignment - remainder);
}
}
