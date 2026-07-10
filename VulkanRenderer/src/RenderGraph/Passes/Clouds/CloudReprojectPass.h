#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::Reproject
{
struct ExecutionInfo
{
    RG::BufferResource ViewInfo{};
    RG::ImageResource Color{};
    RG::ImageResource Depth{};
    RG::ImageResource SceneDepth{};
    RG::ImageResource ColorAccumulationIn{};
    RG::ImageResource DepthAccumulationIn{};
    RG::ImageResource ReprojectionFactorIn{};
    RG::ImageResource ColorAccumulationOut{};
    RG::ImageResource DepthAccumulationOut{};
    RG::ImageResource ReprojectionFactorOut{};
    RG::BufferResource CloudParameters;
};

struct PassData
{
    RG::ImageResource ColorAccumulationPrevious{};
    RG::ImageResource DepthAccumulationPrevious{};
    RG::ImageResource ReprojectionFactorPrevious{};
    RG::ImageResource ColorAccumulation{};
    RG::ImageResource DepthAccumulation{};
    RG::ImageResource ReprojectionFactor{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
