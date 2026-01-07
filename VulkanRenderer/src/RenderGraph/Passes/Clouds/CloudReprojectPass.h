#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::Reproject
{
struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource Color{};
    RG::Resource Depth{};
    RG::Resource SceneDepth{};
    RG::Resource ColorAccumulationIn{};
    RG::Resource DepthAccumulationIn{};
    RG::Resource ReprojectionFactorIn{};
    RG::Resource ColorAccumulationOut{};
    RG::Resource DepthAccumulationOut{};
    RG::Resource ReprojectionFactorOut{};
    RG::Resource CloudParameters;
};

struct PassData
{
    RG::Resource ColorAccumulationPrevious{};
    RG::Resource DepthAccumulationPrevious{};
    RG::Resource ReprojectionFactorPrevious{};
    RG::Resource ColorAccumulation{};
    RG::Resource DepthAccumulation{};
    RG::Resource ReprojectionFactor{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
