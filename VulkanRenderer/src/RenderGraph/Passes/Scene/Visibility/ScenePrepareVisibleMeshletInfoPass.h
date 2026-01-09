#pragma once

#include "RenderGraph/RGResource.h"

class SceneRenderObjectSet;

namespace Passes::ScenePrepareVisibleMeshletInfo
{
struct ExecutionInfo
{
    const SceneRenderObjectSet* RenderObjectSet{nullptr};
};

struct PassData
{
    RG::Resource MeshletInfos{};
    RG::Resource MeshletInfoCount{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
