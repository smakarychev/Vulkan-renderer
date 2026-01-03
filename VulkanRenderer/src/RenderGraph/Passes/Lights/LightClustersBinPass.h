#pragma once

#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::LightClustersBin
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource Clusters{};
    RG::Resource ClusterVisibility{};
    RG::Resource Depth{};
    const SceneLight* Light{nullptr};
};

struct PassData
{
    RG::Resource Clusters{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
