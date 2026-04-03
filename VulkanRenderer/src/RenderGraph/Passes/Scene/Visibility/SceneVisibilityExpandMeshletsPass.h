#pragma once
#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RGResource.h"

namespace Passes::SceneVisibilityExpandMeshlets
{
struct ExecutionInfo
{
    SceneVisibilityPassesResources* Resources{nullptr};
    SceneVisibilityStage Stage{SceneVisibilityStage::Cull};
};
struct PassData
{
    SceneVisibilityPassesResources* Resources{nullptr};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}


