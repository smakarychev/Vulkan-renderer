#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Fxaa
{
struct ExecutionInfo
{
    RG::Resource Color{};
};

struct PassData
{
    RG::Resource AntiAliased{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
