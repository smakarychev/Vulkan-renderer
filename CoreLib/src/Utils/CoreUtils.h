#pragma once

#include "types.h"

namespace CoreUtils
{
    /* NOTE: alignment is a power of 2 */
    inline u64 align(u64 sizeBytes, u64 alignment)
    {
        u64 mask = alignment - 1;
        if (alignment != 0)
            return (sizeBytes + mask) & ~mask;
        return sizeBytes;
    }
}
