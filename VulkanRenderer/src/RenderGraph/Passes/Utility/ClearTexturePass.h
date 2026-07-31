#pragma once
#include "RenderGraph/RGResource.h"
#include "Rendering/RenderingInfo.h"

namespace Passes::ClearColorTexture
{
struct ExecutionInfo
{
    RG::ImageResource Image{};
    ColorClearValue ClearValue{};
};

struct PassData
{
    RG::ImageResource Image{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}


namespace Passes::ClearDepthTexture
{
struct ExecutionInfo
{
    RG::ImageResource Image{};
    DepthStencilClearValue ClearValue{};
};

struct PassData
{
    RG::ImageResource Image{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}

