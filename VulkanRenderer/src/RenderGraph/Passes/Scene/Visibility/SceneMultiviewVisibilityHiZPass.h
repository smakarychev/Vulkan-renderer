#pragma once

#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RenderPass.h"

namespace Passes::SceneMultiviewVisibilityHiz
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* MultiviewVisibility{nullptr};
        SceneVisibilityPassesResources* Resources{nullptr};
        std::array<RG::Resource, SceneMultiviewVisibility::MAX_VIEWS> Depths{};
        std::array<ImageSubresourceDescription, SceneMultiviewVisibility::MAX_VIEWS> Subresources{};
    };
    struct PassData
    {
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
