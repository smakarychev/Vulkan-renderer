#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight2;

namespace Passes::LightClustersBin
{
    struct ExecutionInfo
    {
        RG::Resource DispatchIndirect{};
        RG::Resource Clusters{};
        RG::Resource ActiveClusters{};
        RG::Resource ClustersCount{};
        const SceneLight2* Light{nullptr};
    };
    struct PassData
    {
        RG::Resource Clusters{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
