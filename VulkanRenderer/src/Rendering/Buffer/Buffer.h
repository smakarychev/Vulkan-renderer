#pragma once

#include "Rendering/ResourceHandle.h"
#include "BufferTraits.h"
#include "Rendering/CommandBuffer.h"

#include <CoreLib/Containers/Span.h>

struct BufferSubresourceDescription;
struct BufferDescription;

struct BufferTag{};
struct Buffer : ResourceHandleType<BufferTag>
{
    void Resize(u64 newSize, CommandBuffer cmd, bool copyData = true) const;
    void* Map() const;
    void Unmap() const;
    void SetData(Span<const std::byte> data, u64 offsetBytes) const;
    static void SetData(void* mappedAddress, Span<const std::byte> data, u64 offsetBytes);
    void* GetMappedAddress() const;
    usize GetSizeBytes() const;
    template <typename T>
    Span<const T> GetView(const BufferSubresourceDescription& subresource) const;
    const BufferDescription& GetDescription() const;
    u64 GetDeviceAddress() const;
};

struct BufferSubresourceDescription
{
    static constexpr u64 WHOLE_SIZE{~0ull};
    u64 SizeBytes{WHOLE_SIZE};
    u64 Offset{0};
};

struct BufferSubresource
{
    Buffer Buffer{};
    BufferSubresourceDescription Description{};
};

using BufferBinding = BufferSubresource;

struct BufferDescription
{
    u64 SizeBytes{0};
    BufferUsage Usage{BufferUsage::None};
};

struct BufferCreateInfo
{
    BufferDescription Description{};
    bool PersistentMapping{false};
    Span<const std::byte> InitialData{};
};


template <typename T>
Span<const T> Buffer::GetView(const BufferSubresourceDescription& subresource) const
{
    return Span<const T>(
        (const T*)((const u8*)GetMappedAddress() + subresource.Offset, subresource.SizeBytes) / sizeof(T));
}