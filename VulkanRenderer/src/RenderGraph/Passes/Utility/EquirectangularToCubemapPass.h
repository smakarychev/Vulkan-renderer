#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::EquirectangularToCubemap
{
struct ExecutionInfo
{
    RG::ImageResource Equirectangular{};
    RG::ImageResource Cubemap{};
    f32 Exposure{1.0f};
};
struct PassData
{
    RG::ImageResource Cubemap{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
