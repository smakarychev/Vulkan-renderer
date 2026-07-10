#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::Compose
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource SceneColor{};
    RG::ImageResource SceneDepth{};
    RG::ImageResource CloudColor{};
    RG::ImageResource CloudDepth{};
};

struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
