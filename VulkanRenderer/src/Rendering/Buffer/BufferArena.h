#pragma once

#include "Buffer.h"
#include "Rendering/ResourceHandle.h"

#include <expected>

struct BufferArenaCreateInfo
{
    Buffer Buffer{};
    u64 VirtualSizeBytes{};
};

struct BufferArenaTag{};
using BufferArena = ResourceHandleType<BufferArenaTag>;

using BufferSuballocationHandle = u64;

enum class BufferSuballocationError : u8
{
    OutOfPhysicalMemory,
    OutOfVirtualMemory
};

struct BufferSuballocation
{
    Buffer Buffer{};
    BufferSubresourceDescription Description{};
    BufferSuballocationHandle Handle{};
};

using BufferSuballocationResult = std::expected<BufferSuballocation, BufferSuballocationError>;
