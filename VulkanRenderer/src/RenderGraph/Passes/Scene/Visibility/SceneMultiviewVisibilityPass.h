#pragma once

#include "RenderGraph/RGResource.h"
#include "RenderGraph/Passes/Scene/SceneDrawPassesCommon.h"

class SceneMultiviewVisibility;

namespace Passes::SceneMultiviewVisibility
{
    struct ExecutionInfo
    {
        ::SceneMultiviewVisibility* Visibility{nullptr};
        std::vector<SceneBucketDrawPassInfo> BucketPasses{};
    };
    struct PassData
    {
    };
    RG::Pass& addToGraph(StringId name, RG::Graph& renderGraph, ExecutionInfo& info);
}
