#pragma once

#include "SceneVisibilityPassesCommon.h"
#include "RenderGraph/RGPass.h"

namespace Passes::SceneMultiviewCreateDrawCommands
{
struct ExecutionInfo
{
    ::SceneMultiviewVisibility* MultiviewVisibility{nullptr};
    SceneVisibilityPassesResources* Resources{nullptr};
    SceneVisibilityStage Stage{SceneVisibilityStage::Cull};
    u64 BucketsMask{~0lu};
};

struct PassData
{
    SceneVisibilityPassesResources* Resources{nullptr};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
