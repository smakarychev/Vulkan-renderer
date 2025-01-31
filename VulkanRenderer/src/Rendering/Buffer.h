#pragma once

#include "types.h"

#include "ResourceHandle.h"
#include "Common/Span.h"
#include "core.h"

class DeletionQueue;

enum class BufferUsage
{
    None                    = 0,
    Vertex                  = BIT(1),
    Index                   = BIT(2),
    Uniform                 = BIT(3),
    Storage                 = BIT(4),
    Indirect                = BIT(5),
    Mappable                = BIT(6),
    MappableRandomAccess    = BIT(7),
    Source                  = BIT(8),
    Destination             = BIT(9),
    Conditional             = BIT(10),
    DeviceAddress           = BIT(11),

    Staging                 = Source | Mappable,
    StagingRandomAccess     = Source | MappableRandomAccess,
    Readback                = MappableRandomAccess,

    /* most buffers need this, reads like 'ordinary vertex', 'ordinary storage', etc. */
    Ordinary                = Destination | DeviceAddress,
};

CREATE_ENUM_FLAGS_OPERATORS(BufferUsage)

namespace BufferUtils
{
    std::string bufferUsageToString(BufferUsage usage);
}

struct BufferCreateInfo
{
    u64 SizeBytes{0};
    BufferUsage Usage{BufferUsage::None};
    bool PersistentMapping{false};
    Span<const std::byte> InitialData{};
};

struct BufferTag{};
using Buffer = ResourceHandleType<BufferTag>;

struct BufferDescription
{
    u64 SizeBytes{0};
    BufferUsage Usage{BufferUsage::None};
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