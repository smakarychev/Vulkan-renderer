#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::CloudCurlNoise
{
    struct ExecutionInfo
    {
        /* optional external cloud curl noise image */
        Image CloudCurlNoise{};
    };
    struct PassData
    {
        RG::Resource CloudCurlNoise{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);   
}
