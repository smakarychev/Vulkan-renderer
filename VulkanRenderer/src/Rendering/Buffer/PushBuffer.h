#pragma once

#include "Buffer.h"

struct PushBuffer
{
    Buffer Buffer{};
    u64 Offset{};
};

template <typename T>
struct PushBufferTyped
{
    Buffer Buffer{};
    u32 Offset{};
};