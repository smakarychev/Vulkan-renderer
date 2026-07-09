#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::VP::ProfileMap
{
struct ExecutionInfo
{
    /* optional external profile map image */
    RG::Resource ProfileMap{};
};

struct PassData
{
    RG::Resource ProfileMap{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
