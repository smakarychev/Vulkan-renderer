#pragma once

#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RenderPass.h"

namespace Passes::SceneMultiviewRenderObjectVisibility
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
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
