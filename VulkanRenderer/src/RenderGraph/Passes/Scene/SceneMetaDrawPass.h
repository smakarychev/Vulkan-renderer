#pragma once

#include "Visibility/SceneVisibilityPassesCommon.h"
#include "RenderGraph/RenderPass.h"
#include "SceneDrawPassesCommon.h"

namespace Passes::SceneMetaDraw
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* Visibility{nullptr};
        SceneVisibilityPassesResources* Resources{nullptr};
        std::vector<SceneViewDrawPassInfo> ViewPasses{};
    };
    struct PassData
    {
        
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
