#pragma once

#include "Core/CoreUtils.h"
#include "Vulkan/Driver.h"

namespace renderUtils
{
    inline u64 alignUniformBufferSizeBytes(u64 sizeBytes)
    {
        return CoreUtils::align(sizeBytes, Driver::GetUniformBufferAlignment());
    }

    template <typename T>
    u64 alignUniformBufferSizeBytes(u32 frames)
    {
        u64 alignment = Driver::GetUniformBufferAlignment();
        u64 mask = alignment - 1;
        u64 sizeBytes = sizeof(T);
        if (alignment != 0) // intel gpu has 0 alignment
            sizeBytes = (sizeBytes + mask) & ~mask;
        return sizeBytes * frames;
    }
}
