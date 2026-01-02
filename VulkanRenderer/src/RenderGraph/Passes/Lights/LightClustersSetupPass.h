#pragma once
#include "RenderGraph/RGResource.h"

class Camera;

namespace Passes::LightClustersSetup
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
};
struct PassData
{
    RG::Resource Clusters;
    RG::Resource ClusterVisibility;
};
PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
