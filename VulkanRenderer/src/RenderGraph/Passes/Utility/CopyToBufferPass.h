#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::CopyToBuffer
{
struct ExecutionInfo
{
    Span<const std::byte> Source{};
    RG::BufferResource Destination{};
    u64 DestinationOffset{0};
};

struct PassData
{
    RG::BufferResource Destination{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
