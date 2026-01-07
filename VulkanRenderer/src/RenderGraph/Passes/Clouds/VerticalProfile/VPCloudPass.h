#pragma once
#include "RenderGraph/RGResource.h"
#include "RenderGraph/Passes/Generated/Types/CloudsVPParametersUniform.generated.h"

namespace Passes::Clouds::VP
{
enum class CloudsRenderingMode : u8
{
    FullResolution,
    Reprojection,
};

struct CloudParameters : gen::CloudsVPParameters
{
};

struct ExecutionInfo
{
    RG::Resource ViewInfo{};
    RG::Resource CloudCoverage{};
    RG::Resource CloudProfile{};
    RG::Resource CloudShapeLowFrequencyMap{};
    RG::Resource CloudShapeHighFrequencyMap{};
    RG::Resource CloudCurlNoise{};
    RG::Resource DepthIn{};
    RG::Resource MinMaxDepthIn{};
    /* optional external color target resource */
    RG::Resource ColorOut{};
    /* optional external depth target resource */
    RG::Resource DepthOut{};
    RG::Resource AerialPerspectiveLut{};
    RG::Resource IrradianceSH{};
    RG::Resource CloudParameters{};
    CloudsRenderingMode CloudsRenderingMode{CloudsRenderingMode::FullResolution};
    bool IsEnvironmentCapture{false};
};

struct PassData
{
    RG::Resource ColorOut{};
    RG::Resource DepthOut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
