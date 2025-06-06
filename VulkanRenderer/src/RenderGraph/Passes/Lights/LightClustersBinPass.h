#pragma once

#include "RenderGraph/RGDrawResources.h"
#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::LightClustersBin
{
    struct ExecutionInfo
    {
        RG::Resource DispatchIndirect{};
        RG::Resource Clusters{};
        RG::Resource ActiveClusters{};
        RG::Resource ClustersCount{};
        const SceneLight* Light{nullptr};
    };
    struct PassData
    {
        RG::Resource Clusters{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
