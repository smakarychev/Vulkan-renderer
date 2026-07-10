#pragma once
#include "RenderGraph/RGResource.h"

class Camera;

namespace Passes::LightClustersSetup
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
};

struct PassData
{
    RG::BufferResource Clusters;
    RG::BufferResource ClusterVisibility;
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
