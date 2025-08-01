#pragma once
#include "RenderGraph/RGResource.h"

namespace Passes::Clouds::VP
{
    struct CloudParameters;
}

namespace Passes::CloudReproject
{
    struct ExecutionInfo
    {
        RG::Resource ViewInfo{};
        RG::Resource Color{};
        RG::Resource Depth{};
        RG::Resource ColorAccumulationIn{};
        RG::Resource DepthAccumulationIn{};
        RG::Resource ReprojectionFactorIn{};
        RG::Resource ColorAccumulationOut{};
        RG::Resource DepthAccumulationOut{};
        RG::Resource ReprojectionFactorOut{};
        const Clouds::VP::CloudParameters* CloudParameters{nullptr};
    };
    struct PassData
    {
        RG::Resource ViewInfo{};
        RG::Resource Color{};
        RG::Resource Depth{};
        RG::Resource ColorAccumulationIn{};
        RG::Resource DepthAccumulationIn{};
        RG::Resource ReprojectionFactorIn{};
        RG::Resource ColorAccumulationOut{};
        RG::Resource DepthAccumulationOut{};
        RG::Resource ReprojectionFactorOut{};
    };
    PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
