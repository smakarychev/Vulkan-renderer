#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::BRDFLut
{
struct ExecutionInfo
{
    RG::Resource Lut{};
};

struct PassData
{
    RG::Resource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);

TextureDescription getLutDescription();
}
