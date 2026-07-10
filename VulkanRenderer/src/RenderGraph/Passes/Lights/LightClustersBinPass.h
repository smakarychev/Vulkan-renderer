#pragma once

#include "RenderGraph/RGResource.h"

class SceneLight;

namespace Passes::LightClustersBin
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::BufferResource Clusters{};
    RG::BufferResource ClusterVisibility{};
    RG::ImageResource Depth{};
    RG::BufferResource PointLights{};
};

struct PassData
{
    RG::BufferResource Clusters{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
