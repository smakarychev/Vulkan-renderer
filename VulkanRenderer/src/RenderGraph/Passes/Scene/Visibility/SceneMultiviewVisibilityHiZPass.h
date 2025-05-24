#pragma once

#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RGPass.h"

namespace Passes::SceneMultiviewVisibilityHiz
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* MultiviewVisibility{nullptr};
        SceneVisibilityPassesResources* Resources{nullptr};
        std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> Depths{};
    };
    struct PassData
    {
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
