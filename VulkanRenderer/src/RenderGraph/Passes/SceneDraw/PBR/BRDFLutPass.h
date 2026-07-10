#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::BRDFLut
{
struct ExecutionInfo
{
    RG::ImageResource Lut{};
};

struct PassData
{
    RG::ImageResource Lut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);

TextureDescription getLutDescription();
}
