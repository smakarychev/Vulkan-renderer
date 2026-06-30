#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EquirectangularToCubemap
{
struct ExecutionInfo
{
    RG::Resource Equirectangular{};
    RG::Resource Cubemap{};
    f32 Exposure{1.0f};
};
struct PassData
{
    RG::Resource Cubemap{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
