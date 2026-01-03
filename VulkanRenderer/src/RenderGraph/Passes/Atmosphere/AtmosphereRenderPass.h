#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Render
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource SkyViewLut{};
    RG::Resource AerialPerspective{};
    RG::Resource ColorIn{};
    RG::Resource DepthIn{};
    bool IsPrimaryView{false};
};

struct PassData
{
    RG::Resource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
