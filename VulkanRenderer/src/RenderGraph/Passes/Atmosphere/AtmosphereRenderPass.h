#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Atmosphere::Render
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource SkyViewLut{};
    RG::ImageResource AerialPerspective{};
    RG::ImageResource ColorIn{};
    RG::ImageResource DepthIn{};
    bool IsPrimaryView{false};
};

struct PassData
{
    RG::ImageResource Color{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
