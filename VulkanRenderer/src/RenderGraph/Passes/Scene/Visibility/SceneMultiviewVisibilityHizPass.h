#pragma once

#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RenderPass.h"

namespace Passes::SceneMultiviewVisibilityHiz
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* Visibility{nullptr};
        SceneVisibilityPassesResources* Resources{nullptr};
        SceneVisibilityStage Stage{SceneVisibilityStage::Cull};
    };
    struct PassData
    {
        
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, ExecutionInfo& info);
}
