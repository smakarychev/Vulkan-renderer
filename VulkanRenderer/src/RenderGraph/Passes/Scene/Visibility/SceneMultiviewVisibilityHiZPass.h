#pragma once

#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RenderPass.h"

namespace Passes::SceneMultiviewVisibilityHiz
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* Visibility{nullptr};
        SceneVisibilityPassesResources* Resources{nullptr};
        std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> Depths{};
        std::array<ImageSubresourceDescription, SceneMultiviewVisibility::MAX_VIEWS> Subresources{};
        /* array of optionally pregenerated HiZs */
        std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> HiZs{};
    };
    struct PassData
    {
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
