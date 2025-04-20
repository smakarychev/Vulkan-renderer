#pragma once

#include "RenderGraph/Passes/Scene/Visibility/SceneVisibilityPassesCommon.h"
#include "RenderGraph/RenderPass.h"
#include "SceneDrawPassesCommon.h"

namespace Passes::SceneMetaDraw
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* MultiviewVisibility{nullptr};
        SceneVisibilityPassesResources* Resources{nullptr};
        std::vector<SceneDrawPassDescription> DrawPasses{};
    };
    struct PassData
    {
        SceneDrawPassViewAttachments DrawPassViewAttachments{};
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
