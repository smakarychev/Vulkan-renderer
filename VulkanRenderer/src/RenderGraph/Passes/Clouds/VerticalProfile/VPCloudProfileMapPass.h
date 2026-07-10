#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::VP::ProfileMap
{
struct ExecutionInfo
{
    /* optional external profile map image */
    RG::ImageResource ProfileMap{};
};

struct PassData
{
    RG::ImageResource ProfileMap{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
