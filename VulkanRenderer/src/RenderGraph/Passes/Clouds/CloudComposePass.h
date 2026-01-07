#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::Compose
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource SceneColor{};
    RG::Resource SceneDepth{};
    RG::Resource CloudColor{};
    RG::Resource CloudDepth{};
};

struct PassData
{
    RG::Resource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
