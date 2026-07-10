#pragma once

#include "RenderGraph/RGResource.h"

namespace Passes::Crt
{
struct ExecutionInfo
{
    RG::ImageResource Color{};
};
struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
