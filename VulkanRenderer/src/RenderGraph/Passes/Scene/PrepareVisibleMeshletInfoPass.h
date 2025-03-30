#pragma once
#include "RenderGraph/RGResource.h"

class SceneGeometry2;
class SceneRenderObjectSet;

namespace Passes::PrepareVisibleMeshletInfo
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
    RG::Pass& addToGraph(std::string_view name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
