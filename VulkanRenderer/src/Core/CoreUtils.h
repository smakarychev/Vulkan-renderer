#pragma once

#include "types.h"

namespace CoreUtils
{
    inline u64 align(u64 sizeBytes, u64 alignment)
    {
        u64 mask = alignment - 1;
        if (alignment != 0) // intel gpu has 0 alignment
            return (sizeBytes + mask) & ~mask;
        return sizeBytes;
    }
}
