﻿#pragma once

#include "Rendering/ResourceHandle.h"
#include "BufferTraits.h"
#include "Containers/Span.h"

struct BufferCreateInfo
{
    u64 SizeBytes{0};
    BufferUsage Usage{BufferUsage::None};
    bool PersistentMapping{false};
    Span<const std::byte> InitialData{};
};

struct BufferTag{};
using Buffer = ResourceHandleType<BufferTag>;

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