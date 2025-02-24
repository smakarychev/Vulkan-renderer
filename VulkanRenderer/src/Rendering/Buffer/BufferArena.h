#pragma once

#include "Buffer.h"
#include "Rendering/ResourceHandle.h"

struct BufferArenaCreateInfo
{
    Buffer Buffer{};
};

struct BufferArenaTag{};
using BufferArena = ResourceHandleType<BufferArenaTag>;

using BufferSuballocationHandle = u64;

struct BufferSuballocation
{
    Buffer Buffer{};
    BufferSubresourceDescription Description{};
    BufferSuballocationHandle Handle{};
};