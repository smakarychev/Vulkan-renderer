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
constexpr BufferSuballocationHandle INVALID_BUFFER_SUBALLOCATION_HANDLE = ~0llu;

enum class BufferSuballocationError : u8
{
    OutOfPhysicalMemory,
    OutOfVirtualMemory
};

struct BufferSuballocation
{
    Buffer Buffer{};
    BufferSubresourceDescription Description{};
    BufferSuballocationHandle Handle{INVALID_BUFFER_SUBALLOCATION_HANDLE};
};

using BufferSuballocationResult = std::expected<BufferSuballocation, BufferSuballocationError>;
