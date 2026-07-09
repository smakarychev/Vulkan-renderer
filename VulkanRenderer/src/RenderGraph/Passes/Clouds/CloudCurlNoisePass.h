#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::CurlNoise
{
struct ExecutionInfo
{
    /* optional external cloud curl noise image */
    RG::Resource CloudCurlNoise{};
};

struct PassData
{
    RG::Resource CloudCurlNoise{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
