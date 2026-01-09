#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::Crt
{
struct ExecutionInfo
{
    RG::Resource Color{};
};
struct PassData
{
    RG::Resource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
