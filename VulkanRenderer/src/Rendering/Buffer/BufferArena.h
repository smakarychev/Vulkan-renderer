#pragma once

#include "Buffer.h"
#include "Rendering/ResourceHandle.h"

#include <expected>

struct BufferArenaCreateInfo
{
    Buffer Buffer{};
    u64 VirtualSizeBytes{};
};

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

struct BufferArenaTag{};
struct BufferArena : ResourceHandleType<BufferArenaTag>
{
    void ResizePhysical(u64 newSize, CommandBuffer cmd, bool copyData = true) const;
    Buffer GetUnderlyingBuffer() const;
    u64 GetSizeBytesPhysical() const;
    BufferSuballocationResult Suballocate(u64 sizeBytes, u32 alignment = 8) const;
    void Free(BufferSuballocationHandle suballocation) const;
};
