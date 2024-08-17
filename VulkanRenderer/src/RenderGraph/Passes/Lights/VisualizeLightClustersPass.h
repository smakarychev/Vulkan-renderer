#pragma once
#include "RenderGraph/RGResource.h"

class Camera;

namespace Passes::LightClustersVisualize
{
    struct PassData
    {
        RG::Resource Depth{};
        RG::Resource ColorOut{};
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, RG::Resource depth);
}
