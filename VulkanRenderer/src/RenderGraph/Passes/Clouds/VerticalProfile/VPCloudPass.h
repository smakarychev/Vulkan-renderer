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
    RG::BufferResource ViewInfo{};
    RG::ImageResource CloudCoverage{};
    RG::ImageResource CloudProfile{};
    RG::ImageResource CloudShapeLowFrequencyMap{};
    RG::ImageResource CloudShapeHighFrequencyMap{};
    RG::ImageResource CloudCurlNoise{};
    RG::ImageResource DepthIn{};
    RG::ImageResource MinMaxDepthIn{};
    /* optional external color target resource */
    RG::ImageResource ColorOut{};
    /* optional external depth target resource */
    RG::ImageResource DepthOut{};
    RG::ImageResource AerialPerspectiveLut{};
    RG::BufferResource IrradianceSH{};
    RG::BufferResource CloudParameters{};
    CloudsRenderingMode CloudsRenderingMode{CloudsRenderingMode::FullResolution};
    bool IsEnvironmentCapture{false};
};

struct PassData
{
    RG::ImageResource ColorOut{};
    RG::ImageResource DepthOut{};
};

PassData& addToGraph(StringId name, RG::Graph& renderGraph, const ExecutionInfo& info);
}
