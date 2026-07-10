#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Fxaa
{
struct ExecutionInfo
{
    RG::ImageResource Color{};
};

struct PassData
{
    RG::ImageResource AntiAliased{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
