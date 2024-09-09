#pragma once
#include "RenderGraph/RGResource.h"

class Camera;

namespace Passes::LightClustersSetup
{
    struct PassData
    {
        RG::Resource Clusters;
        RG::Resource ClusterVisibility;
    };
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph);
}
