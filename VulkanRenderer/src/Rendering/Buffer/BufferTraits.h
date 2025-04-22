#pragma once

#include "core.h"
#include "types.h"

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

namespace BufferTraits
{
    std::string bufferUsageToString(BufferUsage usage);
}