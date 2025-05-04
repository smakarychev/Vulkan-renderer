#pragma once

#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RenderPass.h"

namespace Passes::SceneMultiviewMeshletVisibility
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* MultiviewVisibility{nullptr};
        SceneVisibilityPassesResources* Resources{nullptr};
        SceneVisibilityStage Stage{SceneVisibilityStage::Cull};
    };
    struct PassData
    {
        SceneVisibilityPassesResources* Resources{nullptr};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}