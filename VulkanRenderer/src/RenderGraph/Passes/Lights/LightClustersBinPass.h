#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::LightClustersBin
{
    struct PassData
    {
        RG::Resource Dispatch{};
        RG::Resource Clusters{};
        RG::Resource ActiveClusters{};
        RG::Resource ClusterCount{};
        RG::SceneLightResources SceneLightResources{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph,
        RG::Resource dispatchIndirect, RG::Resource clusters, RG::Resource activeClusters, RG::Resource clustersCount,
        const SceneLight& sceneLight);
}
