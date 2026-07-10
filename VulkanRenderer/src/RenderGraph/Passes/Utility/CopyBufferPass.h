#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::CopyBuffer
{
struct ExecutionInfo
{
    RG::BufferResource Source{};
    RG::BufferResource Destination{};
    u64 SizeBytes{0};
    u64 SourceOffset{0};
    u64 DestinationOffset{0};
};

struct PassData
{
    RG::BufferResource Source{};
    RG::BufferResource Destination{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
